// termacs — geometry primitives (all units are terminal CELLS)
#pragma once
#include <algorithm>

namespace termacs {

struct Point { int x = 0, y = 0; };
struct Size  { int w = 0, h = 0; };

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    int  right()  const { return x + w; }
    int  bottom() const { return y + h; }
    bool empty()  const { return w <= 0 || h <= 0; }
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    Rect inset(int d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }

    // Intersection of two rects (empty if disjoint).
    Rect operator&(const Rect& o) const {
        int nx = std::max(x, o.x), ny = std::max(y, o.y);
        int nr = std::min(right(), o.right()), nb = std::min(bottom(), o.bottom());
        return {nx, ny, std::max(0, nr - nx), std::max(0, nb - ny)};
    }
};

} // namespace termacs
