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
        t.set(Role::Accent,        S(yellow, blue, true));
        t.set(Role::Muted,         S(gray, blue));
    } else if (b == Builtin::Light) {
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
        t.set(Role::Accent,        S(Color{180, 140, 0}, brightWhite, true));
        t.set(Role::Muted,         S(gray, brightWhite));
    } else { // PhosphorHarmony — green base, amber accent, gray info (§5.6)
        const Color bg{40, 40, 40};      // dark gray screen
        const Color dimG{0, 90, 30};     // dim phosphor — borders, inactive chrome
        const Color midG{51, 200, 80};   // mid phosphor — normal text
        const Color brightG{140, 255, 140}; // bright phosphor — focus highlight
        const Color selBg{0, 70, 25};    // deep-green selection fill
        const Color amber{220, 190, 40};
        const Color amberB{250, 230, 120}; // primary / accent / titles
        const Color grayI{150, 160, 150};  // info / muted
        const Color amberOrange{255, 140, 80};
        t.set(Role::WindowBg,      S(midG, bg));
        t.set(Role::WindowFrame,   S(dimG, bg));
        t.set(Role::WindowTitle,   S(amberB, bg, true));
        t.set(Role::MenuBar,       S(midG, selBg));
        t.set(Role::MenuBarSel,    S(bg, amberB, true));
        t.set(Role::MenuBg,        S(midG, selBg));
        t.set(Role::MenuItem,      S(midG, selBg));
        t.set(Role::MenuItemSel,   S(bg, amberB, true));
        t.set(Role::Label,         S(midG, bg));
        t.set(Role::Button,        S(midG, bg));
        t.set(Role::ButtonFocused, S(amberB, bg, true));
        t.set(Role::Input,         S(midG, bg));
        t.set(Role::InputFocused,  S(brightG, bg));
        t.set(Role::ListItem,      S(midG, bg));
        t.set(Role::ListSel,       S(brightG, selBg));
        t.set(Role::StatusBar,     S(grayI, selBg));
        t.set(Role::DialogBg,      S(midG, bg));
        t.set(Role::DialogFrame,   S(dimG, bg));
        t.set(Role::DialogTitle,   S(amberB, bg, true));
        t.set(Role::ButtonPrimary, S(amberB, bg, true));
        t.set(Role::ButtonDanger,  S(amberOrange, bg, true));
        t.set(Role::ButtonQuiet,   S(dimG, bg));
        t.set(Role::Option,        S(midG, bg));
        t.set(Role::OptionFocused, S(amberB, bg, true));
        t.set(Role::Combo,         S(midG, bg));
        t.set(Role::ComboFocused,  S(amberB, bg, true));
        t.set(Role::DropdownBg,    S(midG, selBg));
        t.set(Role::DropdownSel,   S(bg, amberB, true));
        t.set(Role::ProgressTrack, S(dimG, bg));
        t.set(Role::ProgressFill,  S(bg, midG));
        t.set(Role::FrameBorder,   S(dimG, bg));
        t.set(Role::FrameTitle,    S(amberB, bg, true));
        t.set(Role::ScrollTrack,   S(dimG, bg));
        t.set(Role::ScrollThumb,   S(brightG, bg));
        t.set(Role::Accent,        S(amber, bg, true));
        t.set(Role::Muted,         S(grayI, bg));
    }
    return t;
}

} // namespace termacs
