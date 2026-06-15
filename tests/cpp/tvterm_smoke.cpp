// Exercises the REAL tvision terminal backend path (setUpConsole, present,
// pollInput, shutdown). Must be run under a pty (see CTest wrapper / `script`).
#include "termacs/cell.hpp"
#include "termacs/tvision_backend.hpp"
#include <cstdio>

using namespace termacs;

int main() {
    auto be = makeTerminalBackend();
    Size s = be->size();
    CellBuffer buf(s.w, s.h);
    buf.clear(Style{colors::white, colors::blue});
    Canvas cv(buf, {0, 0, s.w, s.h});
    cv.box({0, 0, s.w, s.h}, Style{colors::brightWhite, colors::blue});
    cv.drawText(2, 0, " termacs ", Style{colors::brightWhite, colors::blue, true});
    cv.drawText(2, 2, "Terminal backend OK", Style{colors::brightGreen, colors::blue});
    be->present(buf);
    InputEvent ev;
    be->pollInput(ev, 50);     // brief, non-interactive
    be->shutdown();
    std::fprintf(stderr, "tvterm ok %dx%d\n", s.w, s.h);
    return 0;
}
