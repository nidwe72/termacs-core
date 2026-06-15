// termacs — the cell buffer (what backends present) and Canvas (clipped drawing API)
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "geometry.hpp"
#include "style.hpp"

namespace termacs {

struct Cell {
    char32_t ch = U' ';
    Style    style{};
    bool     wideTrail = false;   // right half of a double-width glyph
};

// Display width of a codepoint in cells (0 / 1 / 2). Minimal wcwidth.
int charWidth(char32_t c);
// Display width of a UTF-8 string in cells.
int textWidth(std::string_view utf8);

class CellBuffer {
public:
    CellBuffer() = default;
    CellBuffer(int w, int h) { resize(w, h); }

    void resize(int w, int h) {
        w_ = w < 0 ? 0 : w;
        h_ = h < 0 ? 0 : h;
        cells_.assign(static_cast<size_t>(w_) * h_, Cell{});
    }
    int  width()  const { return w_; }
    int  height() const { return h_; }
    bool inBounds(int x, int y) const { return x >= 0 && x < w_ && y >= 0 && y < h_; }

    Cell&       at(int x, int y)       { return cells_[static_cast<size_t>(y) * w_ + x]; }
    const Cell& at(int x, int y) const { return cells_[static_cast<size_t>(y) * w_ + x]; }

    void clear(Style s = {}) { for (auto& c : cells_) c = Cell{U' ', s, false}; }

    // UTF-8 text of one row, trailing blanks trimmed (for snapshot tests).
    std::string rowText(int y) const;

private:
    int               w_ = 0, h_ = 0;
    std::vector<Cell> cells_;
};

// A clipped, offset drawing surface over a CellBuffer.
class Canvas {
public:
    Canvas(CellBuffer& buf, Rect clip) : buf_(buf), clip_(clip & Rect{0, 0, buf.width(), buf.height()}) {}

    Rect clip() const { return clip_; }

    void set(int x, int y, char32_t ch, Style s);
    // Draw UTF-8 text starting at (x,y); returns cells advanced.
    int  drawText(int x, int y, std::string_view utf8, Style s);
    void fill(Rect r, Style s);
    void hline(int x, int y, int len, char32_t ch, Style s);
    void vline(int x, int y, int len, char32_t ch, Style s);
    // Box-drawing frame around r (single line).
    void box(Rect r, Style s);

    // A sub-canvas clipped to r (intersected with the current clip).
    Canvas sub(Rect r) const { return Canvas(buf_, r & clip_); }

private:
    CellBuffer& buf_;
    Rect        clip_;
};

} // namespace termacs
