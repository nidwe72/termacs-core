#include "termacs/cell.hpp"

namespace termacs {

int charWidth(char32_t c) {
    if (c == 0) return 0;
    // minimal double-width detection (CJK / fullwidth ranges)
    if ((c >= 0x1100 && c <= 0x115F) ||   // Hangul Jamo
        (c >= 0x2E80 && c <= 0xA4CF) ||   // CJK ... Yi
        (c >= 0xAC00 && c <= 0xD7A3) ||   // Hangul syllables
        (c >= 0xF900 && c <= 0xFAFF) ||   // CJK compat
        (c >= 0xFF00 && c <= 0xFF60) ||   // fullwidth forms
        (c >= 0x1F300 && c <= 0x1FAFF))   // emoji/symbols
        return 2;
    return 1;
}

namespace {
void encodeUtf8(char32_t c, std::string& out) {
    if (c < 0x80) {
        out.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (c >> 6)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (c >> 12)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (c >> 18)));
        out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
}

// Decode one codepoint from utf8 at pos; advance pos. Returns U+FFFD on error.
char32_t decodeUtf8(std::string_view s, size_t& pos) {
    unsigned char c = static_cast<unsigned char>(s[pos]);
    char32_t cp;
    int extra;
    if (c < 0x80)        { cp = c;        extra = 0; }
    else if (c < 0xE0)   { cp = c & 0x1F; extra = 1; }
    else if (c < 0xF0)   { cp = c & 0x0F; extra = 2; }
    else                 { cp = c & 0x07; extra = 3; }
    ++pos;
    for (int i = 0; i < extra && pos < s.size(); ++i, ++pos)
        cp = (cp << 6) | (static_cast<unsigned char>(s[pos]) & 0x3F);
    return cp;
}
} // namespace

int textWidth(std::string_view s) {
    int w = 0;
    size_t pos = 0;
    while (pos < s.size()) w += charWidth(decodeUtf8(s, pos));
    return w;
}

std::string CellBuffer::rowText(int y) const {
    std::string out;
    if (y < 0 || y >= h_) return out;
    for (int x = 0; x < w_; ++x) {
        const Cell& c = at(x, y);
        if (c.wideTrail) continue;
        encodeUtf8(c.ch, out);
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

void Canvas::set(int x, int y, char32_t ch, Style s) {
    if (!clip_.contains(x, y)) return;
    if (!buf_.inBounds(x, y)) return;
    Cell& cell = buf_.at(x, y);
    cell.ch = ch;
    cell.style = s;
    cell.wideTrail = false;
    if (charWidth(ch) == 2 && clip_.contains(x + 1, y) && buf_.inBounds(x + 1, y)) {
        Cell& trail = buf_.at(x + 1, y);
        trail.ch = U' ';
        trail.style = s;
        trail.wideTrail = true;
    }
}

int Canvas::drawText(int x, int y, std::string_view utf8, Style s) {
    int cx = x;
    size_t pos = 0;
    while (pos < utf8.size()) {
        char32_t cp = decodeUtf8(utf8, pos);
        int w = charWidth(cp);
        if (w == 0) continue;
        set(cx, y, cp, s);
        cx += w;
    }
    return cx - x;
}

void Canvas::fill(Rect r, Style s) {
    Rect a = r & clip_;
    for (int y = a.y; y < a.bottom(); ++y)
        for (int x = a.x; x < a.right(); ++x)
            if (buf_.inBounds(x, y)) { Cell& c = buf_.at(x, y); c.ch = U' '; c.style = s; c.wideTrail = false; }
}

void Canvas::hline(int x, int y, int len, char32_t ch, Style s) {
    for (int i = 0; i < len; ++i) set(x + i, y, ch, s);
}
void Canvas::vline(int x, int y, int len, char32_t ch, Style s) {
    for (int i = 0; i < len; ++i) set(x, y + i, ch, s);
}

void Canvas::box(Rect r, Style s) {
    if (r.w < 2 || r.h < 2) return;
    int x0 = r.x, y0 = r.y, x1 = r.right() - 1, y1 = r.bottom() - 1;
    set(x0, y0, U'┌', s); set(x1, y0, U'┐', s);   // ┌ ┐
    set(x0, y1, U'└', s); set(x1, y1, U'┘', s);   // └ ┘
    hline(x0 + 1, y0, r.w - 2, U'─', s);               // ─
    hline(x0 + 1, y1, r.w - 2, U'─', s);
    vline(x0, y0 + 1, r.h - 2, U'│', s);               // │
    vline(x1, y0 + 1, r.h - 2, U'│', s);
}

} // namespace termacs
