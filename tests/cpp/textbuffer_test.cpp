// T1: TextBuffer model (§5.11) — movement, selection, word-nav, multi-line,
// clipboard ops, UTF-8 codepoint correctness. No rendering.
#include "../../src/text_buffer.hpp"
#include <cassert>
#include <iostream>
using namespace termacs;

int main() {
    TextBuffer b;
    b.insert("hello world");
    assert(b.text() == "hello world" && b.cursor() == 11);

    b.moveHome(false); assert(b.cursor() == 0);
    b.moveEnd(false);  assert(b.cursor() == 11);
    b.moveWordLeft(false); assert(b.cursor() == 6);   // start of "world"
    b.moveWordLeft(false); assert(b.cursor() == 0);   // start of "hello"
    b.moveWordRight(false); assert(b.cursor() == 6);  // next word start

    // shift-select a word, then type to replace it
    b.moveDocStart(false);
    b.moveWordRight(true);
    assert(b.hasSelection() && b.selectedText() == "hello");
    b.insert("HI");
    assert(b.text() == "HI world" && !b.hasSelection());

    // select-all + cut + paste
    b.selectAll();
    assert(b.cut() == "HI world" && b.text() == "");
    b.paste("abc"); assert(b.text() == "abc");

    // backspace / delete
    b.moveDocEnd(false); b.backspace(); assert(b.text() == "ab");
    b.moveDocStart(false); b.del();     assert(b.text() == "b");

    // multi-line
    TextBuffer m; m.setMultiline(true);
    m.setText("line1\nline2\nthird");
    assert(m.lineCount() == 3 && m.lineAt(1) == "line2");
    m.moveDocStart(false);
    m.moveDown(false);
    { auto rc = m.cursorRowCol(); assert(rc.first == 1 && rc.second == 0); }
    m.moveEnd(false);
    assert(m.cursorRowCol().second == 5);             // "line2".length
    m.moveDown(false);
    assert(m.cursorRowCol().first == 2);              // "third"
    m.moveDocStart(false); m.deleteWordRight();        // removes "line1\n"
    assert(m.lineCount() == 2 && m.lineAt(0) == "line2");

    // UTF-8: positions are codepoints
    TextBuffer u; u.setText("héllo");
    assert(u.length() == 5);
    u.moveDocEnd(false); u.backspace();
    assert(u.text() == "héll");

    // single-line strips newlines on insert
    TextBuffer s;
    s.insert("a\nb");
    assert(s.text() == "ab");

    std::cout << "textbuffer OK\n";
    return 0;
}
