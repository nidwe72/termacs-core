// termacs — role-based typed theme (§5.6). No QSS; widgets resolve a Style by Role.
#pragma once
#include <array>
#include <cstddef>
#include "style.hpp"

namespace termacs {

enum class Role {
    WindowBg, WindowFrame, WindowTitle,
    MenuBar, MenuBarSel,
    MenuBg, MenuItem, MenuItemSel,
    Label,
    Button, ButtonFocused,
    Input, InputFocused,
    ListItem, ListSel,
    StatusBar,
    DialogBg, DialogFrame, DialogTitle,
    // P5 widgets (§5.10)
    ButtonPrimary, ButtonDanger, ButtonQuiet,
    Option, OptionFocused,
    Combo, ComboFocused,
    DropdownBg, DropdownSel,
    ProgressTrack, ProgressFill,
    FrameBorder, FrameTitle,
    ScrollTrack, ScrollThumb,
    _Count
};

class Theme {
public:
    enum class Builtin { Dark, Light };

    Style get(Role r) const { return styles_[static_cast<std::size_t>(r)]; }
    void  set(Role r, Style s) { styles_[static_cast<std::size_t>(r)] = s; }

    static Theme builtin(Builtin b);

private:
    std::array<Style, static_cast<std::size_t>(Role::_Count)> styles_{};
};

} // namespace termacs
