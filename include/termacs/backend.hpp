// termacs — the Backend seam (§6). The core talks ONLY to this; no termacs widget
// type and no tvision type appears above it. tvision and Headless are implementations.
#pragma once
#include <string_view>
#include "cell.hpp"
#include "input.hpp"

namespace termacs {

enum class ColorDepth { Mono, Ansi16, Xterm256, TrueColor };

struct Capabilities {
    ColorDepth color = ColorDepth::TrueColor;
    bool       mouse = false;
    bool       boxGlyphs = true;   // can render box-drawing/block glyphs
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual Size         size() = 0;                 // width/height in CELLS
    virtual Capabilities caps() = 0;
    virtual int          measureWidth(std::string_view utf8) = 0;  // display cells
    virtual void         present(const CellBuffer&) = 0;           // blit a frame
    // Block up to timeoutMs for input; fill `out` and return true if an event arrived.
    virtual bool         pollInput(InputEvent& out, int timeoutMs) = 0;
    virtual void         wake() = 0;                 // thread-safe: unblock pollInput
    virtual void         setCursor(int x, int y, bool visible) {} // optional
    virtual void         setClipboard(std::string_view) {}        // OSC 52 (§5.11.4); optional
    virtual void         shutdown() = 0;             // restore terminal; panic-safe
};

} // namespace termacs
