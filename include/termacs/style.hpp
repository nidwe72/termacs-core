// termacs — colors & cell styling (truecolor; backend downsamples per Capabilities)
#pragma once
#include <cstdint>

namespace termacs {

// 24-bit color with a "terminal default" sentinel.
struct Color {
    uint8_t r = 0, g = 0, b = 0;
    bool    isDefault = true;
    constexpr Color() = default;
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_), isDefault(false) {}
    static constexpr Color def() { return Color(); }
    bool operator==(const Color& o) const {
        return isDefault == o.isDefault && r == o.r && g == o.g && b == o.b;
    }
};

namespace colors {
inline constexpr Color black{0, 0, 0};
inline constexpr Color white{192, 192, 192};
inline constexpr Color brightWhite{255, 255, 255};
inline constexpr Color blue{0, 0, 170};
inline constexpr Color brightBlue{60, 90, 220};
inline constexpr Color cyan{0, 170, 170};
inline constexpr Color brightCyan{85, 255, 255};
inline constexpr Color green{0, 170, 0};
inline constexpr Color brightGreen{85, 255, 85};
inline constexpr Color red{170, 0, 0};
inline constexpr Color yellow{255, 255, 85};
inline constexpr Color gray{85, 85, 85};
inline constexpr Color darkGray{40, 40, 40};
} // namespace colors

struct Style {
    Color fg{};
    Color bg{};
    bool  bold = false;
    bool  underline = false;
    bool  reverse = false;
};

// Decoration style for the "actuator" widgets — Button, ComboBox, LineEdit (§5.12).
// Orthogonal to the color Theme: this is glyphs/geometry, the theme is colors.
// `Inherit` (per-widget default) resolves to the app-wide global default.
enum class ControlStyle : int { Inherit = 0, Plain = 1, Brackets = 2, Framed = 3 };

} // namespace termacs
