// termacs — INTERNAL: the node tree + Core registry. Not an installed header.
// Public facades (widget.hpp) resolve to these nodes through Core.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "termacs/backend.hpp"
#include "termacs/cell.hpp"
#include "termacs/handle.hpp"
#include "termacs/signal.hpp"
#include "termacs/sizing.hpp"
#include "termacs/theme.hpp"
#include "text_buffer.hpp"

namespace termacs {

class Core;

enum class NodeKind { Window, VBox, HBox, Label, Button, LineEdit, ListView, MenuBar, Menu, StatusBar, Dialog,
                      CheckBox, OptionGroup, ComboBox, ProgressBar, TextArea, Frame, ScrollView };

// ---------------------------------------------------------------------------
class Node {
public:
    explicit Node(NodeKind k) : kind(k) {}
    virtual ~Node() = default;

    NodeKind            kind;
    NodeId              self{};
    NodeId              parent{};
    std::vector<NodeId> children;
    Rect                bounds{};
    Sizing              hsize{}, vsize{};
    bool                visible = true, enabled = true, focusable = false;
    ControlStyle        ctrlStyle = ControlStyle::Inherit;  // §5.12; Inherit ⇒ use Core's global

    Sizing& sizing(Axis a) { return a == Axis::Horizontal ? hsize : vsize; }

    virtual Size contentSize(Core&, Backend&)          { return {0, 0}; }
    virtual void arrange(Core&, Backend&)              {}
    virtual void draw(Core&, Canvas&, const Theme&)    {}
    virtual bool handleKey(Core&, const KeyEvent&)     { return false; }
    virtual void onRemoved()                           {}
};

// preferred extent of a node along one axis (clamped to its sizing)
int prefAlong(Node& n, Core& core, Backend& be, Axis ax);

// ---------------------------------------------------------------------------
class ContainerNode : public Node {
public:
    explicit ContainerNode(NodeKind k)
        : Node(k), main_(k == NodeKind::VBox ? Axis::Vertical : Axis::Horizontal) {}
    int  padding = 0;
    int  spacing = 0;
    Axis main_;
    Axis mainAxis() const { return main_; }

    Size contentSize(Core&, Backend&) override;
    void arrange(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
};

// Bordered, titled container (decoration). Children inset by the 1-cell border.
class FrameNode : public ContainerNode {
public:
    FrameNode() : ContainerNode(NodeKind::Frame) { main_ = Axis::Vertical; padding = 1; }
    std::string title;
    void draw(Core&, Canvas&, const Theme&) override;
};

// Clipping viewport that scrolls a single (taller/wider) content child.
class ScrollViewNode : public ContainerNode {
public:
    ScrollViewNode() : ContainerNode(NodeKind::ScrollView) { main_ = Axis::Vertical; focusable = true; }
    int offX = 0, offY = 0;        // scroll offset (cells)
    int contentW = 0, contentH = 0;
    void arrange(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
};

class LabelNode : public Node {
public:
    LabelNode() : Node(NodeKind::Label) {}
    std::string text;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
};

class ButtonNode : public Node {
public:
    ButtonNode() : Node(NodeKind::Button) { focusable = true; }
    std::string text;
    int         variant = 0;       // 0 Normal, 1 Primary, 2 Danger, 3 Quiet
    bool        isDefault = false; // fires on unhandled Enter in its window
    Signal<>    clicked;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
    void onRemoved() override { clicked.clear(); }
};

class LineEditNode : public Node {
public:
    LineEditNode() : Node(NodeKind::LineEdit) { focusable = true; }
    TextBuffer                 buf;          // §5.11 editing model (single-line)
    Signal<const std::string&> textChanged;
    Signal<>                   submitted;
    Signal<>                   selectionChanged;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
    void onRemoved() override { textChanged.clear(); submitted.clear(); selectionChanged.clear(); }
};

class ListViewNode : public Node {
public:
    ListViewNode() : Node(NodeKind::ListView) { focusable = true; }
    std::vector<std::string> items;
    int                      sel = 0;
    int                      top = 0;   // scroll offset
    Signal<int>              activated;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
    void onRemoved() override { activated.clear(); }
};

// ----- P5 selection & input widget nodes (§5.10) ------------------------------
class CheckBoxNode : public Node {
public:
    CheckBoxNode() : Node(NodeKind::CheckBox) { focusable = true; }
    std::string  text;
    bool         checked = false;
    Signal<bool> toggled;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
    void onRemoved() override { toggled.clear(); }
};

class OptionGroupNode : public Node {
public:
    OptionGroupNode() : Node(NodeKind::OptionGroup) { focusable = true; }
    std::vector<std::string> options;
    std::vector<char>        on;        // parallel selected flags
    int   mode = 0;                     // 0 One (radio), 1 Many (check)
    int   cursor = 0;
    Axis  orient = Axis::Vertical;
    Signal<int> selectionChanged;       // i = changed option
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
    void onRemoved() override { selectionChanged.clear(); }
};

class ComboBoxNode : public Node {
public:
    ComboBoxNode() : Node(NodeKind::ComboBox) { focusable = true; }
    std::vector<std::string> options;
    int         sel = -1;
    int         cursor = 0;             // highlighted item while open
    bool        open = false;
    std::string placeholder;
    Rect        popup{};                // dropdown rect (for hit-testing)
    Signal<int> selectionChanged;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
    void onRemoved() override { selectionChanged.clear(); }
};

class ProgressBarNode : public Node {
public:
    ProgressBarNode() : Node(NodeKind::ProgressBar) {}
    int value = 0;                      // 0..100
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
};

class TextAreaNode : public Node {
public:
    TextAreaNode() : Node(NodeKind::TextArea) { buf.setMultiline(true); focusable = true; }
    TextBuffer  buf;                    // §5.11 editing model (multi-line)
    int  top = 0;                       // first visible row (vertical scroll)
    bool readOnly = false;
    bool wrap = false;
    std::string placeholder;
    Signal<const std::string&> textChanged;
    Signal<>                   selectionChanged;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
    void onRemoved() override { textChanged.clear(); selectionChanged.clear(); }
};

struct MenuItem { std::string label; std::function<void()> onActivate; };

class MenuNode : public Node {
public:
    MenuNode() : Node(NodeKind::Menu) {}
    std::string           title;
    std::vector<MenuItem> items;
    int                   sel = 0;
    Rect                  popup{};   // dropdown rect, set during render (for hit-testing)
    // drawn as a popup when open; see Core popup handling
    void draw(Core&, Canvas&, const Theme&) override;
};

class MenuBarNode : public Node {
public:
    MenuBarNode() : Node(NodeKind::MenuBar) {}
    std::vector<NodeId> menus;   // MenuNode ids
    int                 open = -1;   // index of open menu, -1 = none
    int                 hot  = 0;
    void draw(Core&, Canvas&, const Theme&) override;
};

class StatusBarNode : public Node {
public:
    StatusBarNode() : Node(NodeKind::StatusBar) {}
    std::string left, right;
    void draw(Core&, Canvas&, const Theme&) override;
};

class WindowNode : public Node {
public:
    WindowNode() : Node(NodeKind::Window) {}
    std::string title;
    Size        desired{0, 0};   // {0,0} ⇒ fill screen
    NodeId      menuBar{}, statusBar{}, content{};
    void arrange(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
};

// A modal dialog on the floating layer (info / confirm / prompt).
class DialogNode : public Node {
public:
    DialogNode() : Node(NodeKind::Dialog) { focusable = true; }
    std::string                            title, message;
    std::vector<std::string>               buttons;   // e.g. {"Yes","No"} or {"OK"}
    int                                    focusBtn = 0;
    bool                                   hasInput = false;
    std::string                            input;
    std::function<void(int, std::string)>  onClose;   // (buttonIndex, inputText)
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
};

// ---------------------------------------------------------------------------
class Core {
public:
    std::vector<std::unique_ptr<Node>> slots;
    std::vector<uint32_t>              gens;
    std::vector<uint32_t>              freeList;

    Backend*     backend = nullptr;
    Theme        theme = Theme::builtin(Theme::Builtin::Dark);
    ControlStyle controlStyle = ControlStyle::Brackets;   // §5.12 app-wide default

    // Resolve the effective control style for a node (own override, else global).
    ControlStyle styleFor(const Node& n) const {
        return n.ctrlStyle == ControlStyle::Inherit ? controlStyle : n.ctrlStyle;
    }

    std::vector<NodeId> windows;   // root windows (z-order)
    std::vector<NodeId> popups;    // floating layer: open menu / dialogs (top = last)
    NodeId              focused{};
    std::string         clipboard; // internal clipboard register (§5.11.4)

    template <class T, class... A>
    std::pair<NodeId, T*> create(A&&... args) {
        auto node = std::make_unique<T>(std::forward<A>(args)...);
        T* raw = node.get();
        uint32_t idx;
        if (!freeList.empty()) { idx = freeList.back(); freeList.pop_back(); slots[idx] = std::move(node); }
        else { idx = static_cast<uint32_t>(slots.size()); slots.push_back(std::move(node)); gens.push_back(1); }
        NodeId id{idx, gens[idx]};
        raw->self = id;
        return {id, raw};
    }

    Node* resolve(NodeId id) {
        if (id.gen == 0 || id.index >= slots.size()) return nullptr;
        if (gens[id.index] != id.gen || !slots[id.index]) return nullptr;
        return slots[id.index].get();
    }
    Node& require(NodeId id) {
        Node* n = resolve(id);
        if (!n) throw InvalidHandle();
        return *n;
    }
    template <class T> T* as(NodeId id) { return dynamic_cast<T*>(resolve(id)); }

    void addChild(NodeId parent, NodeId child);
    void destroy(NodeId id);

    // focus
    void                setFocus(NodeId id);
    NodeId              activeRoot();                 // top popup, else top window
    std::vector<NodeId> focusOrder(NodeId root);
    void                focusStep(int dir);

    // layout + render
    void layoutAll(Size screen);
    void drawNode(NodeId id, Canvas& cv);
    void render(CellBuffer& out);

    // input
    bool   dispatchKey(const KeyEvent& ev);
    bool   dispatchMouse(const MouseEvent& ev);
    void   dispatchPaste(const std::string& text);   // §5.11.4 bracketed paste
    NodeId hitTest(NodeId root, int x, int y);

private:
    void collectFocusable(NodeId id, std::vector<NodeId>& out);
};

} // namespace termacs
