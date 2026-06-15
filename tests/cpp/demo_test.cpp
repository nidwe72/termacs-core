// P1 exit test: the full TaskDemo (menus, layout, widgets, signals, dialogs) driven
// headlessly. Mirrors termacs-java-demo so the Java port can be diffed against it.
#include "termacs/termacs.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace termacs;

static InputEvent keyEv(Key k) { InputEvent e; e.kind = InputKind::Key; e.key.key = k; return e; }
static InputEvent charEv(char c) {
    InputEvent e; e.kind = InputKind::Key;
    e.key.key = Key::Char; e.key.ch = (char32_t)c; e.key.text = std::string(1, c);
    return e;
}
static bool rowHas(const CellBuffer& fb, const std::string& s) {
    for (int y = 0; y < fb.height(); ++y)
        if (fb.rowText(y).find(s) != std::string::npos) return true;
    return false;
}
static void dump(const CellBuffer& fb, const char* label) {
    std::cout << "--- " << label << " ---\n";
    for (int y = 0; y < fb.height(); ++y) {
        std::string t = fb.rowText(y); t.resize(fb.width(), ' ');
        std::cout << "|" << t << "|\n";
    }
}

int main() {
    Application app(std::make_unique<HeadlessBackend>(64, 20));
    app.setTheme(Theme::builtin(Theme::Builtin::Dark));

    Window win = app.createWindow("termacs — Task Demo");
    win.resize(64, 20);

    MenuBar mb = win.setMenuBar();
    Menu file = mb.addMenu("File");
    Menu help = mb.addMenu("Help");

    bool quit = false;
    file.addItem("Quit", [&] { quit = true; });
    help.addItem("About", [&] { app.dialogs().info(win, "termacs demo"); });

    VBox body = win.setContentVBox();
    body.setPadding(1);
    body.setSpacing(1);

    HBox row = body.addHBox();
    row.addLabel("Task:");
    LineEdit input = row.addLineEdit();
    input.setSizing(Axis::Horizontal, Sizing::of().withPreferred(20).withStretch(1));
    Button add = row.addButton("Add");

    ListView tasks = body.addListView();
    tasks.setSizing(Axis::Vertical, Sizing::of().withMin(3).withStretch(1));

    StatusBar status = win.setStatusBar();

    auto addTask = [&] {
        std::string t = input.text();
        if (t.empty()) return;
        tasks.addItem(t);
        input.setText("");
        status.setText(std::to_string(tasks.count()) + " task(s)");
    };
    add.clicked().connect(addTask);
    input.submitted().connect(addTask);

    status.setText("0 task(s)");
    win.show();
    input.setFocus();

    // --- type "milk" + Enter ---
    for (char c : std::string("milk")) app.feed(charEv(c));
    assert(input.text() == "milk");
    app.feed(keyEv(Key::Enter));
    assert(tasks.count() == 1 && tasks.itemAt(0) == "milk");

    const CellBuffer& fb = app.render();
    dump(fb, "after add");
    assert(rowHas(fb, "milk"));
    assert(rowHas(fb, "1 task(s)"));
    assert(rowHas(fb, "File") && rowHas(fb, "Help"));
    assert(rowHas(fb, "Task:") && rowHas(fb, "[ Add ]"));
    assert(rowHas(fb, "termacs — Task Demo"));

    // --- File ▸ Quit via keyboard (F10 opens, Enter activates first item) ---
    app.feed(keyEv(Key::F10));
    app.feed(keyEv(Key::Enter));
    assert(quit);

    // --- async confirm dialog ---
    bool confirmed = false;
    app.dialogs().confirm(win, "Remove \"milk\"?", [&](bool yes) { confirmed = yes; });
    const CellBuffer& fb2 = app.render();
    dump(fb2, "confirm dialog");
    assert(rowHas(fb2, "Remove \"milk\"?"));
    assert(rowHas(fb2, "Yes") && rowHas(fb2, "No"));
    app.feed(keyEv(Key::Enter));      // focusBtn 0 = Yes
    assert(confirmed);

    std::cout << "P1 demo OK\n";
    return 0;
}
