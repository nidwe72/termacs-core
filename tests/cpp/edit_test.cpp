// T3: modern editing on LineEdit + TextArea (§5.11) — shortcuts, selection,
// clipboard, word-nav, paste events. Drives the widgets via fed key events.
#include "termacs/termacs.hpp"
#include "termacs/headless_backend.hpp"
#include <cassert>
#include <iostream>
#include <memory>
using namespace termacs;

static InputEvent key(Key k, bool shift = false, bool ctrl = false) {
    InputEvent e; e.kind = InputKind::Key; e.key.key = k;
    e.key.mods.shift = shift; e.key.mods.ctrl = ctrl; return e;
}
static InputEvent ch(char32_t c, bool ctrl = false) {
    InputEvent e; e.kind = InputKind::Key; e.key.key = Key::Char; e.key.ch = c;
    e.key.text = std::string(1, (char)c); e.key.mods.ctrl = ctrl; return e;
}
static InputEvent pasteEv(const std::string& t) {
    InputEvent e; e.kind = InputKind::Paste; e.paste = t; return e;
}

int main() {
    Application app(std::make_unique<HeadlessBackend>(40, 12));
    Window win = app.createWindow("edit");
    win.resize(0, 0);
    VBox col = win.setContentVBox();
    LineEdit le = col.addLineEdit();
    TextArea ta = col.addTextArea();
    win.show();

    // --- LineEdit: type, shift-select, copy, paste ---
    le.setFocus();
    for (char c : std::string("hello world")) app.feed(ch(c));
    assert(le.text() == "hello world");
    app.feed(key(Key::Home, /*shift*/true));            // select whole line
    assert(le.selectedText() == "hello world");
    app.feed(ch(U'c', /*ctrl*/true));                    // copy
    app.feed(key(Key::End));                             // collapse to end
    app.feed(ch(U'v', /*ctrl*/true));                    // paste
    assert(le.text() == "hello worldhello world");
    app.feed(ch(U'a', true));                            // select all
    app.feed(ch(U'x', true));                            // cut
    assert(le.text() == "");

    // --- word navigation + shift-ctrl select ---
    le.setText("foo bar baz");
    le.setFocus();
    app.feed(key(Key::End));
    app.feed(key(Key::Left, false, true));               // Ctrl+Left -> start of "baz"
    app.feed(key(Key::Left, true,  true));               // Shift+Ctrl+Left -> select "bar "
    assert(le.selectedText() == "bar ");

    // --- paste event (bracketed paste) ---
    le.setText(""); le.setFocus();
    app.feed(pasteEv("pasted"));
    assert(le.text() == "pasted");

    // --- TextArea: multi-line edit + select-all ---
    ta.setText("ab\ncd");
    ta.setFocus();
    app.feed(ch(U'a', true));
    assert(ta.selectedText() == "ab\ncd");
    app.feed(key(Key::Down));                            // collapse, move down
    app.feed(key(Key::End));
    app.feed(ch('!'));                                   // append to line 2
    assert(ta.text() == "ab\ncd!");
    app.feed(key(Key::Home, true));                      // select "cd!"
    assert(ta.selectedText() == "cd!");

    std::cout << "edit OK\n";
    return 0;
}
