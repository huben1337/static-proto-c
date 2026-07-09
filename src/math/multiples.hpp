#pragma once

#include <bit>
#include <concepts>
#include <cstddef>

#include "../estd/utility.hpp"
#include "../core/SIZE.hpp"

namespace math {

namespace detail {
    template <std::integral T>
    [[nodiscard, gnu::always_inline]] constexpr T last_multiple (T value, T base) {
        return value & ~(base - 1);
    }

    template <std::integral T>
    [[nodiscard, gnu::always_inline]] constexpr T next_multiple (T value, T base) {
        T mask = base - 1;
        return (value + mask) & ~mask;
    }
} // namespace detail

template <std::integral T>
[[nodiscard]] constexpr T last_multiple (T value, SIZE base) {
    return detail::last_multiple<T>(value, base.byte_size());
}


template <std::integral T, size_t base>
[[nodiscard]] constexpr T last_multiple (T value) {
    static_assert(std::has_single_bit(base), "base must be a power of 2");

    return detail::last_multiple<T>(value, base);
}

template <std::integral T, estd::discouraged_annotation>
[[nodiscard]] constexpr T last_multiple (T value, std::type_identity_t<T> base) {
    assert(std::has_single_bit(base));

    return detail::last_multiple<T>(value, base);
}


template <std::integral T>
[[nodiscard]] constexpr T next_multiple (T value, SIZE base) {
    return detail::next_multiple<T>(value, base.byte_size());
}

template <std::integral T, size_t base>
[[nodiscard]] constexpr T next_multiple (T value) {
    static_assert(std::has_single_bit(base), "base must be a power of 2");
    
    return detail::next_multiple<T>(value, base);
}

template <std::integral T, estd::discouraged_annotation>
[[nodiscard]] constexpr T next_multiple (T value, std::type_identity_t<T> base) {
    assert(std::has_single_bit(base));

    return detail::next_multiple<T>(value, base);
}

} // namespace math