// P0 exit test: a VBox/HBox of widgets, laid out and drawn via HeadlessBackend.
#include "termacs/termacs.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace termacs;

static bool anyRowContains(const CellBuffer& fb, const std::string& needle) {
    for (int y = 0; y < fb.height(); ++y)
        if (fb.rowText(y).find(needle) != std::string::npos) return true;
    return false;
}

int main() {
    Application app(std::make_unique<HeadlessBackend>(40, 12));
    app.setTheme(Theme::builtin(Theme::Builtin::Dark));

    Window win = app.createWindow("P0");
    win.resize(0, 0);                       // fill screen

    VBox col = win.setContentVBox();
    col.setPadding(1);
    col.setSpacing(0);

    HBox row = col.addHBox();
    row.addLabel("Name:");
    LineEdit in = row.addLineEdit();
    in.setText("edwin");
    in.setSizing(Axis::Horizontal, Sizing::of().withStretch(1));
    Button ok = row.addButton("OK");

    col.addLabel("footer");

    win.show();

    const CellBuffer& fb = app.render();
    std::cout << "+" << std::string(fb.width(), '-') << "+\n";
    for (int y = 0; y < fb.height(); ++y) {
        std::string t = fb.rowText(y);
        t.resize(fb.width(), ' ');
        std::cout << "|" << t << "|\n";
    }
    std::cout << "+" << std::string(fb.width(), '-') << "+\n";

    assert(anyRowContains(fb, "Name:"));
    assert(anyRowContains(fb, "edwin"));
    assert(anyRowContains(fb, "[ OK ]"));
    assert(anyRowContains(fb, "footer"));
    assert(anyRowContains(fb, "P0"));       // window title

    std::cout << "P0 snapshot OK\n";
    return 0;
}
