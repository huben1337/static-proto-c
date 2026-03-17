#pragma once

#include <concepts>

#include "../base.hpp"
#include "../estd/utility.hpp"
#include "../core/SIZE.hpp"

namespace math {

template <std::integral T>
[[nodiscard]] constexpr T last_multiple (T value, SIZE base) {
    return value & ~(static_cast<T>(base.byte_size()) - 1);
}

template <std::integral T, estd::discouraged_annotation>
[[nodiscard]] constexpr T last_multiple (T value, std::type_identity_t<T> base) {
    static_warn("base must be a power of 2");
    return value & ~(base - 1);
}


template <std::integral T>
[[nodiscard]] constexpr T next_multiple (T value, SIZE base) {
    T mask = static_cast<T>(base.byte_size()) - 1;
    return (value + mask) & ~mask;
}

template <std::integral T, estd::discouraged_annotation>
[[nodiscard]] constexpr T next_multiple (T value, std::type_identity_t<T> base) {
    static_warn("base must be a power of 2");
    T mask = base - 1;
    return (value + mask) & ~mask;
}

} // namespace math