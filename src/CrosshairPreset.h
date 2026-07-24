#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aimpoint {

enum class CrosshairShape {
    Classic,
    Dot,
    Circle,
    Tee,
    Chevron,
    Diagonal,
    Diamond,
    CornerBox,
    Sniper,
    Wings,
    Triad,
    Hexagon
};

enum class Animation {
    None,
    Pulse,
    Rotate
};

struct RgbColor {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
};

struct CrosshairPreset {
    int id{};
    std::wstring name;
    std::wstring family;
    CrosshairShape shape{CrosshairShape::Classic};
    Animation animation{Animation::None};
    RgbColor primary{};
    RgbColor accent{};
    float size{18.0F};
    float gap{5.0F};
    float thickness{2.0F};
    bool outline{true};
    bool centerDot{false};
};

[[nodiscard]] std::vector<CrosshairPreset> makePresetCatalog();
[[nodiscard]] const wchar_t* animationName(Animation animation) noexcept;
[[nodiscard]] bool isMinimalShape(CrosshairShape shape) noexcept;

} // namespace aimpoint
