// termacs — normalized input events (the backend translates raw terminal input to these)
#pragma once
#include <string>
#include <cstdint>

namespace termacs {

enum class Key {
    None, Char,
    Enter, Escape, Tab, BackTab, Backspace, Delete, Insert,
    Up, Down, Left, Right, Home, End, PageUp, PageDown,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12
};

struct Modifiers {
    bool shift = false, ctrl = false, alt = false;
};

struct KeyEvent {
    Key         key = Key::None;
    char32_t    ch  = 0;          // codepoint when key == Char
    std::string text;             // UTF-8 of the typed character(s)
    Modifiers   mods;
};

struct MouseEvent {
    int  x = 0, y = 0;
    bool left = false, right = false;
    int  wheel = 0;               // +up / -down
};

struct ResizeEvent { int w = 0, h = 0; };

enum class InputKind { None, Key, Mouse, Resize };

struct InputEvent {
    InputKind   kind = InputKind::None;
    KeyEvent    key;
    MouseEvent  mouse;
    ResizeEvent resize;
};

} // namespace termacs
