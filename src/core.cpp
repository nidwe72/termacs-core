#include "internal.hpp"
#include <algorithm>

namespace termacs {

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int prefAlong(Node& n, Core& core, Backend& be, Axis ax) {
    Sizing& s = n.sizing(ax);
    Size cs = n.contentSize(core, be);
    int content = (ax == Axis::Horizontal) ? cs.w : cs.h;
    int p = s.preferred > 0 ? s.preferred : content;
    return clampi(p, s.min, std::min(s.max, Unbounded));
}

// ===========================================================================
// Core registry
// ===========================================================================
void Core::addChild(NodeId parent, NodeId child) {
    Node* p = resolve(parent);
    Node* c = resolve(child);
    if (!p || !c) return;
    c->parent = parent;
    p->children.push_back(child);
}

void Core::destroy(NodeId id) {
    Node* n = resolve(id);
    if (!n) return;
    auto kids = n->children;            // copy: recursion mutates
    for (NodeId k : kids) destroy(k);
    n->onRemoved();

    // unlink from parent
    if (Node* p = resolve(n->parent)) {
        auto& ch = p->children;
        ch.erase(std::remove(ch.begin(), ch.end(), id), ch.end());
    }
    auto drop = [&](std::vector<NodeId>& v) { v.erase(std::remove(v.begin(), v.end(), id), v.end()); };
    drop(windows);
    drop(popups);
    if (focused == id) focused = {};

    uint32_t idx = id.index;
    ++gens[idx];                        // invalidate all handles into this slot
    if (gens[idx] == 0) gens[idx] = 1;
    slots[idx].reset();
    freeList.push_back(idx);
}

// ===========================================================================
// Focus
// ===========================================================================
void Core::setFocus(NodeId id) { if (resolve(id)) focused = id; }

void Core::collectFocusable(NodeId id, std::vector<NodeId>& out) {
    Node* n = resolve(id);
    if (!n || !n->visible) return;
    if (n->focusable && n->enabled) out.push_back(id);
    for (NodeId c : n->children) collectFocusable(c, out);
}

std::vector<NodeId> Core::focusOrder(NodeId root) {
    std::vector<NodeId> out;
    collectFocusable(root, out);
    return out;
}

NodeId Core::activeRoot() {
    if (!popups.empty()) return popups.back();
    if (!windows.empty()) return windows.back();
    return {};
}

void Core::focusStep(int dir) {
    if (windows.empty()) return;
    auto order = focusOrder(windows.back());
    if (order.empty()) return;
    int idx = -1;
    for (size_t i = 0; i < order.size(); ++i) if (order[i] == focused) idx = (int)i;
    int next = (idx < 0) ? (dir > 0 ? 0 : (int)order.size() - 1)
                         : (int)((idx + dir + order.size()) % order.size());
    setFocus(order[next]);
}

// ===========================================================================
// Layout
// ===========================================================================
void Core::layoutAll(Size screen) {
    Backend& be = *backend;
    for (NodeId wid : windows) {
        if (WindowNode* w = as<WindowNode>(wid)) {
            int ww = w->desired.w > 0 ? std::min(w->desired.w, screen.w) : screen.w;
            int wh = w->desired.h > 0 ? std::min(w->desired.h, screen.h) : screen.h;
            w->bounds = {(screen.w - ww) / 2, (screen.h - wh) / 2, ww, wh};
            w->arrange(*this, be);
        }
    }
    // dialogs: size to content, centered
    for (NodeId pid : popups) {
        if (DialogNode* dlg = as<DialogNode>(pid)) {
            // measure message lines
            int mw = (int)dlg->title.size();
            size_t start = 0;
            while (start <= dlg->message.size()) {
                size_t nl = dlg->message.find('\n', start);
                std::string line = dlg->message.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
                mw = std::max(mw, be.measureWidth(line));
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            int btnw = 0;
            for (auto& b : dlg->buttons) btnw += be.measureWidth(b) + 5;
            int w = std::max({mw + 6, btnw + 4, 24});
            int lines = 1; for (char c : dlg->message) if (c == '\n') ++lines;
            int h = lines + (dlg->hasInput ? 2 : 0) + 5;
            w = std::min(w, screen.w);
            h = std::min(h, screen.h);
            dlg->bounds = {(screen.w - w) / 2, (screen.h - h) / 2, w, h};
        }
    }
}

void WindowNode::arrange(Core& core, Backend& be) {
    Rect inner = bounds.inset(1);
    int top = inner.y, bot = inner.bottom();
    if (Node* mb = core.resolve(menuBar)) { mb->bounds = {inner.x, top, inner.w, 1}; top += 1; }
    if (Node* sb = core.resolve(statusBar)) { sb->bounds = {inner.x, bot - 1, inner.w, 1}; bot -= 1; }
    if (Node* c = core.resolve(content)) {
        c->bounds = {inner.x, top, inner.w, std::max(0, bot - top)};
        c->arrange(core, be);
    }
}

Size ContainerNode::contentSize(Core& core, Backend& be) {
    Axis main = mainAxis();
    int along = 0, cross = 0, n = 0;
    for (NodeId cid : children) {
        Node* c = core.resolve(cid);
        if (!c || !c->visible) continue;
        int cw = prefAlong(*c, core, be, Axis::Horizontal);
        int ch = prefAlong(*c, core, be, Axis::Vertical);
        int a = (main == Axis::Vertical) ? ch : cw;
        int x = (main == Axis::Vertical) ? cw : ch;
        along += a;
        cross = std::max(cross, x);
        ++n;
    }
    if (n > 1) along += spacing * (n - 1);
    along += 2 * padding;
    cross += 2 * padding;
    return (main == Axis::Vertical) ? Size{cross, along} : Size{along, cross};
}

void ContainerNode::arrange(Core& core, Backend& be) {
    Rect inner = bounds.inset(padding);
    Axis main = mainAxis();
    Axis crossA = (main == Axis::Vertical) ? Axis::Horizontal : Axis::Vertical;

    std::vector<Node*> kids;
    for (NodeId cid : children) {
        Node* c = core.resolve(cid);
        if (c && c->visible) kids.push_back(c);
        else if (c) c->bounds = {0, 0, 0, 0};
    }
    int n = (int)kids.size();
    if (n == 0) return;

    int availMain = ((main == Axis::Vertical) ? inner.h : inner.w) - spacing * (n - 1);
    std::vector<int> size(n), mn(n), str(n);
    int sumPref = 0, totalStretch = 0;
    for (int i = 0; i < n; ++i) {
        size[i] = prefAlong(*kids[i], core, be, main);
        mn[i] = kids[i]->sizing(main).min;
        str[i] = kids[i]->sizing(main).stretch;
        sumPref += size[i];
        totalStretch += str[i];
    }
    if (availMain >= sumPref) {
        int surplus = availMain - sumPref;
        if (totalStretch > 0 && surplus > 0) {
            int given = 0;
            for (int i = 0; i < n; ++i) { int add = surplus * str[i] / totalStretch; size[i] += add; given += add; }
            int rem = surplus - given;
            for (int i = 0; i < n && rem > 0; ++i) if (str[i] > 0) { size[i] += 1; --rem; }
        }
    } else {
        int deficit = sumPref - availMain;
        int room = 0;
        for (int i = 0; i < n; ++i) room += size[i] - mn[i];
        if (room > 0) {
            for (int i = 0; i < n; ++i) {
                int sh = std::min(size[i] - mn[i], deficit * (size[i] - mn[i]) / room);
                size[i] -= sh;
            }
        }
    }

    int pos = (main == Axis::Vertical) ? inner.y : inner.x;
    int crossPos = (main == Axis::Vertical) ? inner.x : inner.y;
    int crossAvail = (main == Axis::Vertical) ? inner.w : inner.h;
    for (int i = 0; i < n; ++i) {
        int crossSize = std::min(crossAvail, kids[i]->sizing(crossA).max);
        if (crossSize <= 0) crossSize = crossAvail;
        if (main == Axis::Vertical) kids[i]->bounds = {crossPos, pos, crossSize, size[i]};
        else                        kids[i]->bounds = {pos, crossPos, size[i], crossSize};
        kids[i]->arrange(core, be);
        pos += size[i] + spacing;
    }
}

void ContainerNode::draw(Core&, Canvas&, const Theme&) { /* transparent */ }

// ===========================================================================
// Leaf widgets
// ===========================================================================
Size LabelNode::contentSize(Core&, Backend& be) { return {be.measureWidth(text), 1}; }
void LabelNode::draw(Core&, Canvas& cv, const Theme& th) {
    cv.drawText(bounds.x, bounds.y, text, th.get(Role::Label));
}

Size ButtonNode::contentSize(Core&, Backend& be) { return {be.measureWidth(text) + 4, 1}; }
void ButtonNode::draw(Core& core, Canvas& cv, const Theme& th) {
    bool foc = core.focused == self;
    Role base = Role::Button;
    switch (variant) { case 1: base = Role::ButtonPrimary; break; case 2: base = Role::ButtonDanger; break; case 3: base = Role::ButtonQuiet; break; }
    Style st = th.get(foc ? Role::ButtonFocused : base);
    if (isDefault) st.bold = true;
    cv.fill(bounds, st);
    std::string label = "[ " + text + " ]";
    cv.drawText(bounds.x, bounds.y, label, st);
}
bool ButtonNode::handleKey(Core&, const KeyEvent& ev) {
    if (ev.key == Key::Enter || (ev.key == Key::Char && ev.ch == U' ')) { clicked.emit(); return true; }
    return false;
}

Size LineEditNode::contentSize(Core&, Backend& be) { return {std::max(be.measureWidth(text) + 1, 6), 1}; }
void LineEditNode::draw(Core& core, Canvas& cv, const Theme& th) {
    bool foc = core.focused == self;
    Style st = th.get(foc ? Role::InputFocused : Role::Input);
    cv.fill(bounds, st);
    cv.drawText(bounds.x, bounds.y, text, st);
    if (foc) {
        int cx = bounds.x + cursor;
        if (cx < bounds.right()) {
            Style cs = st; cs.reverse = true;
            char32_t under = (cursor < (int)text.size()) ? (char32_t)text[cursor] : U' ';
            cv.set(cx, bounds.y, under, cs);
        }
    }
}
bool LineEditNode::handleKey(Core&, const KeyEvent& ev) {
    bool changed = false;
    if (ev.key == Key::Char && ev.ch >= 0x20) {
        text.insert(text.begin() + std::min(cursor, (int)text.size()), (char)ev.ch);
        ++cursor; changed = true;
    } else if (ev.key == Key::Backspace) {
        if (cursor > 0) { text.erase(text.begin() + (cursor - 1)); --cursor; changed = true; }
    } else if (ev.key == Key::Delete) {
        if (cursor < (int)text.size()) { text.erase(text.begin() + cursor); changed = true; }
    } else if (ev.key == Key::Left)  { if (cursor > 0) --cursor; }
    else if (ev.key == Key::Right) { if (cursor < (int)text.size()) ++cursor; }
    else if (ev.key == Key::Home)  { cursor = 0; }
    else if (ev.key == Key::End)   { cursor = (int)text.size(); }
    else if (ev.key == Key::Enter) { submitted.emit(); return true; }
    else return false;
    if (changed) textChanged.emit(text);
    return true;
}

Size ListViewNode::contentSize(Core&, Backend& be) {
    int w = 0;
    for (auto& s : items) w = std::max(w, be.measureWidth(s));
    return {std::max(w, 4), std::max(1, (int)items.size())};
}
void ListViewNode::draw(Core& core, Canvas& cv, const Theme& th) {
    cv.fill(bounds, th.get(Role::ListItem));
    int rows = bounds.h;
    if (sel < top) top = sel;
    if (sel >= top + rows) top = sel - rows + 1;
    if (top < 0) top = 0;
    bool foc = core.focused == self;
    for (int r = 0; r < rows; ++r) {
        int idx = top + r;
        if (idx >= (int)items.size()) break;
        bool selected = (idx == sel);
        Style st = th.get(selected && foc ? Role::ListSel : Role::ListItem);
        Rect row{bounds.x, bounds.y + r, bounds.w, 1};
        cv.fill(row, st);
        cv.drawText(bounds.x, bounds.y + r, items[idx], st);
    }
}
bool ListViewNode::handleKey(Core&, const KeyEvent& ev) {
    if (items.empty()) return false;
    if (ev.key == Key::Up)        { sel = std::max(0, sel - 1); return true; }
    if (ev.key == Key::Down)      { sel = std::min((int)items.size() - 1, sel + 1); return true; }
    if (ev.key == Key::Home)      { sel = 0; return true; }
    if (ev.key == Key::End)       { sel = (int)items.size() - 1; return true; }
    if (ev.key == Key::Enter)     { activated.emit(sel); return true; }
    return false;
}

// ===========================================================================
// P5 selection & input widgets (§5.10)
// ===========================================================================
Size CheckBoxNode::contentSize(Core&, Backend& be) { return {be.measureWidth(text) + 4, 1}; }
void CheckBoxNode::draw(Core& core, Canvas& cv, const Theme& th) {
    bool foc = core.focused == self;
    Style st = th.get(foc ? Role::OptionFocused : Role::Option);
    cv.fill(bounds, st);
    std::string label = std::string(checked ? "[x] " : "[ ] ") + text;
    cv.drawText(bounds.x, bounds.y, label, st);
}
bool CheckBoxNode::handleKey(Core&, const KeyEvent& ev) {
    if (ev.key == Key::Enter || (ev.key == Key::Char && ev.ch == U' ')) { checked = !checked; toggled.emit(checked); return true; }
    return false;
}

Size OptionGroupNode::contentSize(Core&, Backend& be) {
    if (orient == Axis::Vertical) {
        int w = 0; for (auto& s : options) w = std::max(w, be.measureWidth(s) + 4);
        return {std::max(w, 4), std::max(1, (int)options.size())};
    }
    int tot = 0; for (auto& s : options) tot += be.measureWidth(s) + 4 + 1;
    return {std::max(tot, 4), 1};
}
void OptionGroupNode::draw(Core& core, Canvas& cv, const Theme& th) {
    bool foc = core.focused == self;
    cv.fill(bounds, th.get(Role::Option));
    int x = bounds.x, y = bounds.y;
    for (size_t i = 0; i < options.size(); ++i) {
        bool sel = i < on.size() && on[i];
        bool cur = foc && (int)i == cursor;
        Style st = th.get(cur ? Role::OptionFocused : Role::Option);
        const char* mark = (mode == 0) ? (sel ? "(*) " : "( ) ") : (sel ? "[x] " : "[ ] ");
        std::string label = std::string(mark) + options[i];
        if (orient == Axis::Vertical) { cv.drawText(x, y, label, st); ++y; }
        else { cv.drawText(x, y, label, st); x += (int)label.size() + 1; }
    }
}
bool OptionGroupNode::handleKey(Core&, const KeyEvent& ev) {
    if (options.empty()) return false;
    bool vert = orient == Axis::Vertical;
    Key prev = vert ? Key::Up : Key::Left, next = vert ? Key::Down : Key::Right;
    auto selectCur = [&] { std::fill(on.begin(), on.end(), 0); on[cursor] = 1; selectionChanged.emit(cursor); };
    if (ev.key == prev) { cursor = std::max(0, cursor - 1); if (mode == 0) selectCur(); return true; }
    if (ev.key == next) { cursor = std::min((int)options.size() - 1, cursor + 1); if (mode == 0) selectCur(); return true; }
    if ((ev.key == Key::Char && ev.ch == U' ') || (ev.key == Key::Enter && mode == 0)) {
        if (mode == 0) selectCur();
        else { on[cursor] = on[cursor] ? 0 : 1; selectionChanged.emit(cursor); }
        return true;
    }
    return false;
}

Size ComboBoxNode::contentSize(Core&, Backend& be) {
    int w = be.measureWidth(placeholder);
    for (auto& s : options) w = std::max(w, be.measureWidth(s));
    return {std::max(w + 4, 8), 1};
}
void ComboBoxNode::draw(Core& core, Canvas& cv, const Theme& th) {
    bool foc = core.focused == self;
    Style st = th.get(foc ? Role::ComboFocused : Role::Combo);
    cv.fill(bounds, st);
    std::string label = (sel >= 0 && sel < (int)options.size()) ? options[sel] : placeholder;
    cv.drawText(bounds.x + 1, bounds.y, label, st);
    if (bounds.w >= 3) cv.drawText(bounds.right() - 2, bounds.y, "v", st);
}
bool ComboBoxNode::handleKey(Core&, const KeyEvent& ev) {
    if (open) {
        if (ev.key == Key::Up)    { cursor = std::max(0, cursor - 1); return true; }
        if (ev.key == Key::Down)  { cursor = std::min((int)options.size() - 1, cursor + 1); return true; }
        if (ev.key == Key::Enter || (ev.key == Key::Char && ev.ch == U' ')) { sel = cursor; open = false; selectionChanged.emit(sel); return true; }
        if (ev.key == Key::Escape){ open = false; return true; }
        return true;
    }
    if (ev.key == Key::Enter || (ev.key == Key::Char && ev.ch == U' ')) { if (!options.empty()) { open = true; cursor = sel >= 0 ? sel : 0; } return true; }
    if (ev.key == Key::Up)   { if (sel > 0) { --sel; selectionChanged.emit(sel); } return true; }
    if (ev.key == Key::Down) { if (sel < (int)options.size() - 1) { ++sel; selectionChanged.emit(sel); } return true; }
    return false;
}

Size ProgressBarNode::contentSize(Core&, Backend&) { return {12, 1}; }
void ProgressBarNode::draw(Core&, Canvas& cv, const Theme& th) {
    Style track = th.get(Role::ProgressTrack), fillc = th.get(Role::ProgressFill);
    cv.fill(bounds, track);
    int filled = bounds.w * value / 100;
    if (filled > 0) cv.fill({bounds.x, bounds.y, filled, bounds.h}, fillc);
    std::string lbl = std::to_string(value) + "%";
    int lx = bounds.x + (bounds.w - (int)lbl.size()) / 2;
    for (int i = 0; i < (int)lbl.size(); ++i) {
        int px = lx + i;
        if (px < bounds.x || px >= bounds.right()) continue;
        cv.set(px, bounds.y, (char32_t)lbl[i], (px < bounds.x + filled) ? fillc : track);
    }
}

Size TextAreaNode::contentSize(Core&, Backend& be) {
    int w = 0; for (auto& l : lines) w = std::max(w, be.measureWidth(l));
    return {std::max(w + 1, 10), std::max((int)lines.size(), 3)};
}
void TextAreaNode::draw(Core& core, Canvas& cv, const Theme& th) {
    bool foc = core.focused == self;
    Style st = th.get(foc ? Role::InputFocused : Role::Input);
    cv.fill(bounds, st);
    int rows = bounds.h;
    if (cy < top) top = cy;
    if (cy >= top + rows) top = cy - rows + 1;
    if (top < 0) top = 0;
    bool empty = (lines.size() == 1 && lines[0].empty());
    if (empty && !foc && !placeholder.empty()) { cv.drawText(bounds.x, bounds.y, placeholder, st); return; }
    for (int r = 0; r < rows; ++r) {
        int idx = top + r;
        if (idx >= (int)lines.size()) break;
        cv.drawText(bounds.x, bounds.y + r, lines[idx], st);
    }
    if (foc) {
        int cyr = cy - top;
        if (cyr >= 0 && cyr < rows) {
            int px = bounds.x + cx;
            if (px < bounds.right()) {
                Style cs = st; cs.reverse = true;
                const std::string& ln = lines[cy];
                char32_t under = (cx < (int)ln.size()) ? (char32_t)ln[cx] : U' ';
                cv.set(px, bounds.y + cyr, under, cs);
            }
        }
    }
}
bool TextAreaNode::handleKey(Core&, const KeyEvent& ev) {
    if (readOnly) {
        if (ev.key == Key::Up)   { if (cy > 0) { --cy; cx = std::min(cx, (int)lines[cy].size()); } return true; }
        if (ev.key == Key::Down) { if (cy < (int)lines.size() - 1) { ++cy; cx = std::min(cx, (int)lines[cy].size()); } return true; }
        return false;
    }
    std::string& ln = lines[cy];
    bool changed = false;
    if (ev.key == Key::Char && ev.ch >= 0x20) { ln.insert(ln.begin() + std::min(cx, (int)ln.size()), (char)ev.ch); ++cx; changed = true; }
    else if (ev.key == Key::Enter) {
        std::string rest = ln.substr(std::min(cx, (int)ln.size()));
        ln.erase(std::min(cx, (int)ln.size()));
        lines.insert(lines.begin() + cy + 1, rest); ++cy; cx = 0; changed = true;
    }
    else if (ev.key == Key::Backspace) {
        if (cx > 0) { ln.erase(ln.begin() + (cx - 1)); --cx; changed = true; }
        else if (cy > 0) { cx = (int)lines[cy - 1].size(); lines[cy - 1] += ln; lines.erase(lines.begin() + cy); --cy; changed = true; }
    }
    else if (ev.key == Key::Left)  { if (cx > 0) --cx; else if (cy > 0) { --cy; cx = (int)lines[cy].size(); } }
    else if (ev.key == Key::Right) { if (cx < (int)ln.size()) ++cx; else if (cy < (int)lines.size() - 1) { ++cy; cx = 0; } }
    else if (ev.key == Key::Up)    { if (cy > 0) { --cy; cx = std::min(cx, (int)lines[cy].size()); } }
    else if (ev.key == Key::Down)  { if (cy < (int)lines.size() - 1) { ++cy; cx = std::min(cx, (int)lines[cy].size()); } }
    else if (ev.key == Key::Home)  { cx = 0; }
    else if (ev.key == Key::End)   { cx = (int)ln.size(); }
    else return false;
    if (changed) textChanged.emit(joined());
    return true;
}

// ----- Frame / ScrollView (containers) ----------------------------------------
void FrameNode::draw(Core&, Canvas& cv, const Theme& th) {
    cv.box(bounds, th.get(Role::FrameBorder));
    if (!title.empty() && bounds.w > 4) {
        std::string t = " " + title + " ";
        cv.drawText(bounds.x + 2, bounds.y, t, th.get(Role::FrameTitle));
    }
}
void ScrollViewNode::arrange(Core& core, Backend& be) {
    if (children.empty()) return;
    Node* c = core.resolve(children[0]);
    if (!c) return;
    Size cs = c->contentSize(core, be);
    contentW = std::max(cs.w, bounds.w);
    contentH = std::max(cs.h, 1);
    int maxOffY = std::max(0, contentH - bounds.h);
    offY = std::max(0, std::min(offY, maxOffY));
    int maxOffX = std::max(0, contentW - bounds.w);
    offX = std::max(0, std::min(offX, maxOffX));
    c->bounds = {bounds.x - offX, bounds.y - offY, contentW, contentH};
    c->arrange(core, be);
}
void ScrollViewNode::draw(Core&, Canvas& cv, const Theme& th) {
    cv.fill(bounds, th.get(Role::WindowBg));
    if (contentH > bounds.h && bounds.h > 0) {
        int x = bounds.right() - 1;
        cv.vline(x, bounds.y, bounds.h, U'│', th.get(Role::ScrollTrack));
        int thumbH = std::max(1, bounds.h * bounds.h / contentH);
        int thumbY = bounds.y + (bounds.h - thumbH) * offY / std::max(1, contentH - bounds.h);
        cv.vline(x, thumbY, thumbH, U'█', th.get(Role::ScrollThumb));
    }
}
bool ScrollViewNode::handleKey(Core&, const KeyEvent& ev) {
    int maxOffY = std::max(0, contentH - bounds.h);
    if (ev.key == Key::Down)     { if (offY < maxOffY) ++offY; return true; }
    if (ev.key == Key::Up)       { if (offY > 0) --offY; return true; }
    if (ev.key == Key::PageDown) { offY = std::min(maxOffY, offY + bounds.h); return true; }
    if (ev.key == Key::PageUp)   { offY = std::max(0, offY - bounds.h); return true; }
    if (ev.key == Key::Home)     { offY = 0; return true; }
    if (ev.key == Key::End)      { offY = maxOffY; return true; }
    return false;
}

// ===========================================================================
// Menus & status bar
// ===========================================================================
void MenuNode::draw(Core&, Canvas&, const Theme&) { /* drawn by MenuBar/overlay */ }

void MenuBarNode::draw(Core& core, Canvas& cv, const Theme& th) {
    Style bar = th.get(Role::MenuBar);
    cv.fill(bounds, bar);
    int x = bounds.x + 1;
    for (size_t i = 0; i < menus.size(); ++i) {
        MenuNode* m = core.as<MenuNode>(menus[i]);
        if (!m) continue;
        bool active = ((int)i == open);
        Style st = active ? th.get(Role::MenuBarSel) : bar;
        std::string label = " " + m->title + " ";
        cv.drawText(x, bounds.y, label, st);
        m->bounds = {x, bounds.y, (int)label.size(), 1};   // clickable title region
        x += (int)label.size() + 1;
    }
}

void StatusBarNode::draw(Core&, Canvas& cv, const Theme& th) {
    Style st = th.get(Role::StatusBar);
    cv.fill(bounds, st);
    cv.drawText(bounds.x + 1, bounds.y, left, st);
    int rx = bounds.right() - (int)right.size() - 1;
    if (rx > bounds.x) cv.drawText(rx, bounds.y, right, st);
}

// ===========================================================================
// Window
// ===========================================================================
void WindowNode::draw(Core&, Canvas& cv, const Theme& th) {
    cv.fill(bounds, th.get(Role::WindowBg));
    cv.box(bounds, th.get(Role::WindowFrame));
    if (!title.empty() && bounds.w > 4) {
        std::string t = " " + title + " ";
        int tx = bounds.x + 2;
        cv.drawText(tx, bounds.y, t, th.get(Role::WindowTitle));
    }
}

// ===========================================================================
// Dialog (floating layer)
// ===========================================================================
void DialogNode::draw(Core&, Canvas& cv, const Theme& th) {
    cv.fill(bounds, th.get(Role::DialogBg));
    cv.box(bounds, th.get(Role::DialogFrame));
    if (!title.empty()) {
        std::string t = " " + title + " ";
        cv.drawText(bounds.x + 2, bounds.y, t, th.get(Role::DialogTitle));
    }
    // message lines
    int y = bounds.y + 2;
    size_t start = 0;
    while (start <= message.size()) {
        size_t nl = message.find('\n', start);
        std::string line = message.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        cv.drawText(bounds.x + 3, y++, line, th.get(Role::DialogBg));
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    if (hasInput) {
        Style is = th.get(Role::InputFocused);
        Rect ib{bounds.x + 3, y + 1, bounds.w - 6, 1};
        cv.fill(ib, is);
        cv.drawText(ib.x, ib.y, input, is);
        y += 2;
    }
    // buttons
    int by = bounds.bottom() - 2;
    int totalw = 0;
    for (auto& b : buttons) totalw += (int)b.size() + 4 + 2;
    int bx = bounds.x + (bounds.w - totalw) / 2;
    for (size_t i = 0; i < buttons.size(); ++i) {
        bool foc = ((int)i == focusBtn);
        Style st = th.get(foc ? Role::ButtonFocused : Role::Button);
        std::string label = "[ " + buttons[i] + " ]";
        cv.drawText(bx, by, label, st);
        bx += (int)label.size() + 2;
    }
}
bool DialogNode::handleKey(Core& core, const KeyEvent& ev) {
    if (hasInput && (ev.key == Key::Char || ev.key == Key::Backspace)) {
        if (ev.key == Key::Char && ev.ch >= 0x20) input.push_back((char)ev.ch);
        else if (ev.key == Key::Backspace && !input.empty()) input.pop_back();
        return true;
    }
    auto close = [&](int btn) {
        auto cb = onClose; std::string in = input; NodeId me = self;
        // remove from popups + destroy, then fire callback
        auto& pv = core.popups;
        pv.erase(std::remove(pv.begin(), pv.end(), me), pv.end());
        core.destroy(me);
        if (cb) cb(btn, in);
    };
    if (ev.key == Key::Left || ev.key == Key::BackTab) { focusBtn = (focusBtn - 1 + (int)buttons.size()) % (int)buttons.size(); return true; }
    if (ev.key == Key::Right || ev.key == Key::Tab)    { focusBtn = (focusBtn + 1) % (int)buttons.size(); return true; }
    if (ev.key == Key::Enter) { close(focusBtn); return true; }
    if (ev.key == Key::Escape) { close((int)buttons.size() - 1); return true; }   // last = cancel
    return true;   // modal: swallow everything
}

// ===========================================================================
// Drawing & input dispatch
// ===========================================================================
void Core::drawNode(NodeId id, Canvas& cv) {
    Node* n = resolve(id);
    if (!n || !n->visible) return;
    Canvas sub = cv.sub(n->bounds);
    n->draw(*this, sub, theme);
    for (NodeId c : n->children) drawNode(c, sub);   // clip children to parent (enables ScrollView)
}

void Core::render(CellBuffer& out) {
    Size sc = backend->size();
    out.resize(sc.w, sc.h);
    out.clear(theme.get(Role::WindowBg));
    layoutAll(sc);
    Canvas cv(out, {0, 0, sc.w, sc.h});
    for (NodeId w : windows) drawNode(w, cv);

    // open menu dropdown (overlay, unclipped)
    if (!windows.empty()) {
        if (WindowNode* w = as<WindowNode>(windows.back())) {
            if (MenuBarNode* mb = as<MenuBarNode>(w->menuBar)) {
                if (mb->open >= 0 && mb->open < (int)mb->menus.size()) {
                    if (MenuNode* m = as<MenuNode>(mb->menus[mb->open])) {
                        int mw = 0;
                        for (auto& it : m->items) mw = std::max(mw, (int)it.label.size());
                        mw += 4;
                        Rect db{m->bounds.x, mb->bounds.y + 1, mw, (int)m->items.size() + 2};
                        m->popup = db;             // for mouse hit-testing
                        cv.fill(db, theme.get(Role::MenuBg));
                        cv.box(db, theme.get(Role::MenuBg));
                        for (size_t i = 0; i < m->items.size(); ++i) {
                            bool selrow = ((int)i == m->sel);
                            Style st = theme.get(selrow ? Role::MenuItemSel : Role::MenuItem);
                            Rect row{db.x + 1, db.y + 1 + (int)i, db.w - 2, 1};
                            cv.fill(row, st);
                            cv.drawText(db.x + 2, db.y + 1 + (int)i, m->items[i].label, st);
                        }
                    }
                }
            }
        }
    }
    // open combobox dropdown (overlay, unclipped)
    for (auto& up : slots) {
        auto* cb = dynamic_cast<ComboBoxNode*>(up.get());
        if (!cb || !cb->open || cb->options.empty()) continue;
        Rect db{cb->bounds.x, cb->bounds.bottom(), cb->bounds.w, (int)cb->options.size() + 2};
        cb->popup = db;
        cv.fill(db, theme.get(Role::DropdownBg));
        cv.box(db, theme.get(Role::DropdownBg));
        for (size_t i = 0; i < cb->options.size(); ++i) {
            bool selrow = ((int)i == cb->cursor);
            Style st = theme.get(selrow ? Role::DropdownSel : Role::DropdownBg);
            Rect row{db.x + 1, db.y + 1 + (int)i, db.w - 2, 1};
            cv.fill(row, st);
            cv.drawText(db.x + 1, db.y + 1 + (int)i, cb->options[i], st);
        }
    }

    for (NodeId p : popups) drawNode(p, cv);
}

static ButtonNode* findDefaultBtn(Core& core, NodeId id) {
    Node* n = core.resolve(id);
    if (!n || !n->visible) return nullptr;
    if (auto* b = dynamic_cast<ButtonNode*>(n)) if (b->isDefault && b->enabled) return b;
    for (NodeId c : n->children) if (auto* r = findDefaultBtn(core, c)) return r;
    return nullptr;
}

bool Core::dispatchKey(const KeyEvent& ev) {
    // 1. modal popup (dialog) wins
    if (!popups.empty()) {
        if (Node* p = resolve(popups.back())) return p->handleKey(*this, ev);
    }
    // 2. menu bar interaction
    WindowNode* w = windows.empty() ? nullptr : as<WindowNode>(windows.back());
    MenuBarNode* mb = w ? as<MenuBarNode>(w->menuBar) : nullptr;
    if (mb) {
        if (mb->open >= 0) {
            MenuNode* m = as<MenuNode>(mb->menus[mb->open]);
            int nm = (int)mb->menus.size();
            if (ev.key == Key::Left)  { mb->open = (mb->open - 1 + nm) % nm; return true; }
            if (ev.key == Key::Right) { mb->open = (mb->open + 1) % nm; return true; }
            if (m && ev.key == Key::Up)   { m->sel = (m->sel - 1 + (int)m->items.size()) % (int)m->items.size(); return true; }
            if (m && ev.key == Key::Down) { m->sel = (m->sel + 1) % (int)m->items.size(); return true; }
            if (ev.key == Key::Enter) {
                std::function<void()> fn;
                if (m && m->sel < (int)m->items.size()) fn = m->items[m->sel].onActivate;
                mb->open = -1;
                if (fn) fn();
                return true;
            }
            if (ev.key == Key::Escape) { mb->open = -1; return true; }
            return true;
        }
        if (ev.key == Key::F10) { mb->open = 0; if (MenuNode* m0 = as<MenuNode>(mb->menus.empty() ? NodeId{} : mb->menus[0])) m0->sel = 0; return true; }
    }
    // 3. focus traversal
    if (ev.key == Key::Tab)     { focusStep(+1); return true; }
    if (ev.key == Key::BackTab) { focusStep(-1); return true; }
    // 4. focused widget
    if (Node* f = resolve(focused)) { if (f->handleKey(*this, ev)) return true; }
    // 5. default button fires on an otherwise-unhandled Enter
    if (ev.key == Key::Enter) {
        if (ButtonNode* db = findDefaultBtn(*this, activeRoot())) { db->clicked.emit(); return true; }
    }
    return false;
}

NodeId Core::hitTest(NodeId id, int x, int y) {
    Node* n = resolve(id);
    if (!n || !n->visible || !n->bounds.contains(x, y)) return {};
    for (auto it = n->children.rbegin(); it != n->children.rend(); ++it) {
        NodeId h = hitTest(*it, x, y);
        if (resolve(h)) return h;            // deepest/topmost child wins
    }
    return id;
}

bool Core::dispatchMouse(const MouseEvent& ev) {
    if (!ev.left) return false;              // act on left press only
    if (!popups.empty()) return false;       // dialogs are keyboard-driven for now
    WindowNode* w = windows.empty() ? nullptr : as<WindowNode>(windows.back());
    if (!w) return false;
    int mx = ev.x, my = ev.y;

    if (MenuBarNode* mb = as<MenuBarNode>(w->menuBar)) {
        // 1. click inside an open dropdown -> activate that item
        if (mb->open >= 0) {
            if (MenuNode* m = as<MenuNode>(mb->menus[mb->open])) {
                if (m->popup.contains(mx, my)) {
                    int row = my - (m->popup.y + 1);
                    if (row >= 0 && row < (int)m->items.size()) {
                        auto fn = m->items[row].onActivate;
                        mb->open = -1;
                        if (fn) fn();
                    }
                    return true;
                }
            }
        }
        // 2. click on a menu title -> open/close that menu
        if (Node* mbn = resolve(w->menuBar)) {
            if (my == mbn->bounds.y) {
                for (size_t i = 0; i < mb->menus.size(); ++i) {
                    MenuNode* m = as<MenuNode>(mb->menus[i]);
                    if (m && mx >= m->bounds.x && mx < m->bounds.x + m->bounds.w) {
                        mb->open = (mb->open == (int)i) ? -1 : (int)i;
                        if (mb->open >= 0) m->sel = 0;
                        return true;
                    }
                }
            }
        }
        // 3. click elsewhere closes an open menu
        if (mb->open >= 0) { mb->open = -1; return true; }
    }

    // 4. click to focus / activate a widget
    NodeId hit = hitTest(windows.back(), mx, my);
    if (Node* n = resolve(hit)) {
        if (n->focusable && n->enabled) {
            setFocus(hit);
            if (auto* b = as<ButtonNode>(hit)) b->clicked.emit();
            else if (auto* lv = as<ListViewNode>(hit)) {
                int r = lv->top + (my - n->bounds.y);
                if (r >= 0 && r < (int)lv->items.size()) lv->sel = r;
            }
            return true;
        }
    }
    return false;
}

} // namespace termacs
