#pragma once

#include <cassert>
#include <concepts>
#include <gsl/util>

namespace math {

/**
 * @brief Calculates the 1-based modulo of an unsigned integer
 * @param a The dividend
 * @param n The non zero divisor
 * @return The 1-based modulo
 * @note Terminates program if divisor is zero
 */
template <std::unsigned_integral T, std::unsigned_integral U>
[[nodiscard]] constexpr U mod1 (T a, U n) {
    assert(n != 0);
    if (a == 0) return n;
    return gsl::narrow_cast<U>(((a - 1) % n) + 1);
}

}