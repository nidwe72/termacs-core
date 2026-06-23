// ControlStyle (§5.12) + PhosphorHarmony theme — HeadlessBackend snapshot asserts.
// Each actuator (Button/ComboBox/LineEdit) honours the resolved style; per-widget
// overrides beat the global default; the green theme renders the same glyph structure.
#include "termacs/termacs.hpp"
#include "termacs/headless_backend.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
using namespace termacs;

static bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

// Build a tiny actuator column under the given global style + theme; return all rows.
static std::string snap(ControlStyle cs, Theme::Builtin th) {
    Application app(std::make_unique<HeadlessBackend>(40, 14));
    app.setTheme(Theme::builtin(th));
    app.setControlStyle(cs);
    Window win = app.createWindow("S"); win.resize(0, 0);
    VBox col = win.setContentVBox(); col.setPadding(0); col.setSpacing(0);
    col.addButton("OK");
    ComboBox c = col.addComboBox(); c.setOptions({"a", "b"}); c.setPlaceholder("Pick");
    LineEdit e = col.addLineEdit(); e.setText("hi");
    win.show();
    const CellBuffer& fb = app.render();
    std::string all;
    for (int y = 0; y < fb.height(); ++y) { all += fb.rowText(y); all += "\n"; }
    return all;
}

int main() {
    // Brackets (the global default) wraps actuators in [ ]
    std::string br = snap(ControlStyle::Brackets, Theme::Builtin::Dark);
    assert(has(br, "[ OK ]"));
    assert(has(br, "Pick"));                 // combo placeholder text

    // Plain — fill only, no brackets
    std::string pl = snap(ControlStyle::Plain, Theme::Builtin::Dark);
    assert(!has(pl, "[ OK ]"));
    assert(has(pl, "OK"));

    // Framed — rounded box (╭) around the actuators
    std::string fr = snap(ControlStyle::Framed, Theme::Builtin::Dark);
    assert(has(fr, "╭"));
    assert(has(fr, "OK"));

    // PhosphorHarmony renders the same glyph structure (color is backend-side)
    std::string ph = snap(ControlStyle::Brackets, Theme::Builtin::PhosphorHarmony);
    assert(has(ph, "[ OK ]"));

    // Per-widget override beats the global default
    {
        Application app(std::make_unique<HeadlessBackend>(40, 14));
        app.setControlStyle(ControlStyle::Plain);     // global = plain
        Window win = app.createWindow("S"); win.resize(0, 0);
        VBox col = win.setContentVBox();
        Button b = col.addButton("OK");
        b.setControlStyle(ControlStyle::Framed);       // ...this one framed
        win.show();
        const CellBuffer& fb = app.render();
        std::string all;
        for (int y = 0; y < fb.height(); ++y) { all += fb.rowText(y); all += "\n"; }
        assert(has(all, "╭"));                          // framed despite global plain
    }

    std::cout << "control-style + phosphor OK\n";
    return 0;
}
