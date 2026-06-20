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
        t.set(Role::ButtonPrimary, S(brightWhite, green, true));
        t.set(Role::ButtonDanger,  S(brightWhite, red, true));
        t.set(Role::ButtonQuiet,   S(brightCyan, blue));
        t.set(Role::Option,        S(brightWhite, blue));
        t.set(Role::OptionFocused, S(brightWhite, green, true));
        t.set(Role::Combo,         S(black, cyan));
        t.set(Role::ComboFocused,  S(brightWhite, green, true));
        t.set(Role::DropdownBg,    S(black, white));
        t.set(Role::DropdownSel,   S(brightWhite, green, true));
        t.set(Role::ProgressTrack, S(brightWhite, gray));
        t.set(Role::ProgressFill,  S(black, green));
        t.set(Role::FrameBorder,   S(brightWhite, blue));
        t.set(Role::FrameTitle,    S(brightCyan, blue, true));
        t.set(Role::ScrollTrack,   S(gray, blue));
        t.set(Role::ScrollThumb,   S(brightWhite, blue));
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
        t.set(Role::ButtonPrimary, S(brightWhite, green, true));
        t.set(Role::ButtonDanger,  S(brightWhite, red, true));
        t.set(Role::ButtonQuiet,   S(blue, brightWhite));
        t.set(Role::Option,        S(black, brightWhite));
        t.set(Role::OptionFocused, S(brightWhite, blue, true));
        t.set(Role::Combo,         S(brightWhite, blue));
        t.set(Role::ComboFocused,  S(brightWhite, blue, true));
        t.set(Role::DropdownBg,    S(black, white));
        t.set(Role::DropdownSel,   S(brightWhite, blue, true));
        t.set(Role::ProgressTrack, S(black, white));
        t.set(Role::ProgressFill,  S(brightWhite, green));
        t.set(Role::FrameBorder,   S(gray, brightWhite));
        t.set(Role::FrameTitle,    S(blue, brightWhite, true));
        t.set(Role::ScrollTrack,   S(gray, brightWhite));
        t.set(Role::ScrollThumb,   S(blue, brightWhite));
    }
    return t;
}

} // namespace termacs
