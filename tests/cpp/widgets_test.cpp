// P5 selection & input widgets (§5.10) — HeadlessBackend snapshot + state asserts.
#include "termacs/termacs.hpp"
#include "termacs/headless_backend.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
using namespace termacs;

static bool anyRowContains(const CellBuffer& fb, const std::string& needle) {
    for (int y = 0; y < fb.height(); ++y)
        if (fb.rowText(y).find(needle) != std::string::npos) return true;
    return false;
}

int main() {
    Application app(std::make_unique<HeadlessBackend>(60, 30));
    app.setTheme(Theme::builtin(Theme::Builtin::Dark));
    Window win = app.createWindow("Gallery");
    win.resize(0, 0);
    VBox col = win.setContentVBox();
    col.setPadding(1); col.setSpacing(0);

    HBox brow = col.addHBox();
    Button ok  = brow.addButton("OK");  ok.setVariant(ButtonVariant::Primary); ok.setDefault(true);
    Button del = brow.addButton("Del"); del.setVariant(ButtonVariant::Danger);

    CheckBox cb = col.addCheckBox("Remember me"); cb.setChecked(true);

    OptionGroup radio = col.addOptionGroup(SelectMode::One);
    radio.setOptions({"Small", "Medium", "Large"}); radio.setSelectedIndex(1);

    OptionGroup multi = col.addOptionGroup(SelectMode::Many);
    multi.setOptions({"Bold", "Italic"}); multi.setSelected(0, true);

    ComboBox combo = col.addComboBox();
    combo.setOptions({"Low", "High"}); combo.setPlaceholder("Choose...");

    ProgressBar pb = col.addProgressBar(); pb.setValue(50);

    Frame fr = col.addFrame("Notes");
    TextArea ta = fr.addTextArea(); ta.setText("hello\nworld");

    win.show();
    const CellBuffer& fb = app.render();
    for (int y = 0; y < fb.height(); ++y) {
        std::string t = fb.rowText(y); t.resize(fb.width(), ' ');
        std::cout << "|" << t << "|\n";
    }

    // state
    assert(cb.isChecked());
    assert(radio.selectedIndex() == 1);
    assert(multi.isSelected(0) && !multi.isSelected(1));
    assert(pb.value() == 50);
    assert(combo.selectedText().empty());          // placeholder shown
    assert(ta.text() == "hello\nworld");

    // rendered marks
    assert(anyRowContains(fb, "(*) Medium"));      // radio selected
    assert(anyRowContains(fb, "[x] Bold"));        // multi selected
    assert(anyRowContains(fb, "Choose..."));       // combo placeholder
    assert(anyRowContains(fb, "50%"));             // progress bar
    assert(anyRowContains(fb, "Notes"));           // frame title
    assert(anyRowContains(fb, "hello"));           // text area
    assert(anyRowContains(fb, "[ OK ]"));          // primary button

    std::cout << "widgets snapshot OK\n";
    return 0;
}
