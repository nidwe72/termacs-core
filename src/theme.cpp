#include "termacs/theme.hpp"

namespace termacs {

Theme Theme::builtin(Builtin b) {
    using namespace colors;
    Theme t;
    auto S = [](Color fg, Color bg, bool bold = false) { return Style{fg, bg, bold, false, false}; };

    if (b == Builtin::Dark) {
        t.set(Role::WindowBg,      S(white, blue));
        t.set(Role::WindowFrame,   S(brightWhite, blue));
        t.set(Role::WindowTitle,   S(brightWhite, blue, true));
        t.set(Role::MenuBar,       S(black, white));
        t.set(Role::MenuBarSel,    S(brightWhite, green, true));
        t.set(Role::MenuBg,        S(black, white));
        t.set(Role::MenuItem,      S(black, white));
        t.set(Role::MenuItemSel,   S(brightWhite, green, true));
        t.set(Role::Label,         S(brightWhite, blue));
        t.set(Role::Button,        S(black, cyan));
        t.set(Role::ButtonFocused, S(brightWhite, green, true));
        t.set(Role::Input,         S(black, white));
        t.set(Role::InputFocused,  S(black, brightWhite));
        t.set(Role::ListItem,      S(brightWhite, blue));
        t.set(Role::ListSel,       S(black, cyan));
        t.set(Role::StatusBar,     S(black, white));
        t.set(Role::DialogBg,      S(black, white));
        t.set(Role::DialogFrame,   S(black, white));
        t.set(Role::DialogTitle,   S(blue, white, true));
    } else { // Light
        t.set(Role::WindowBg,      S(black, brightWhite));
        t.set(Role::WindowFrame,   S(gray, brightWhite));
        t.set(Role::WindowTitle,   S(blue, brightWhite, true));
        t.set(Role::MenuBar,       S(black, white));
        t.set(Role::MenuBarSel,    S(brightWhite, blue, true));
        t.set(Role::MenuBg,        S(black, white));
        t.set(Role::MenuItem,      S(black, white));
        t.set(Role::MenuItemSel,   S(brightWhite, blue, true));
        t.set(Role::Label,         S(black, brightWhite));
        t.set(Role::Button,        S(brightWhite, blue));
        t.set(Role::ButtonFocused, S(brightWhite, blue, true));
        t.set(Role::Input,         S(black, white));
        t.set(Role::InputFocused,  S(black, brightWhite));
        t.set(Role::ListItem,      S(black, brightWhite));
        t.set(Role::ListSel,       S(brightWhite, blue));
        t.set(Role::StatusBar,     S(black, white));
        t.set(Role::DialogBg,      S(black, white));
        t.set(Role::DialogFrame,   S(black, white));
        t.set(Role::DialogTitle,   S(blue, white, true));
    }
    return t;
}

} // namespace termacs
