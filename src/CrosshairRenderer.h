#pragma once

#include "CrosshairPreset.h"

#include <windows.h>

#include <cstdint>

namespace aimpoint {

struct RenderOptions {
    float scale{1.0F};
    bool outline{true};
    bool centerDot{false};
    std::uint64_t elapsedMilliseconds{0};
};

class CrosshairRenderer {
public:
    static void draw(HDC dc,
                     POINT center,
                     const CrosshairPreset& preset,
                     const RenderOptions& options);
};

} // namespace aimpoint
