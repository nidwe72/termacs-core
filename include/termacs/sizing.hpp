// termacs — layout sizing & alignment (per §5.3 of the spec)
#pragma once

namespace termacs {

enum class Axis { Horizontal, Vertical };
enum class Align { Start, Center, End, Fill };
using VAlign = Align;

inline constexpr int Unbounded = 1 << 30;

// Per-widget, per-axis sizing constraint.
struct Sizing {
    int min = 0;             // hard floor (cells)
    int preferred = 0;       // natural size; 0 ⇒ ask the widget for its hint
    int max = Unbounded;     // hard ceiling
    int stretch = 0;         // ≥0 weight for sharing surplus space

    static Sizing of() { return {}; }
    Sizing& withMin(int v)       { min = v; return *this; }
    Sizing& withPreferred(int v) { preferred = v; return *this; }
    Sizing& withMax(int v)       { max = v; return *this; }
    Sizing& withStretch(int v)   { stretch = v; return *this; }
};

} // namespace termacs
