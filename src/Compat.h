#pragma once

namespace aimpoint {

template <typename T>
constexpr T clampValue(const T value, const T minimum, const T maximum) noexcept {
    return value < minimum ? minimum : (maximum < value ? maximum : value);
}

} // namespace aimpoint
