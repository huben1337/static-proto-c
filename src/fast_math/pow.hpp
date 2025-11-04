#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <array>

#include "../helper/ce.hpp"


namespace fast_math {

    namespace {
        template <std::unsigned_integral T, T base>
        consteval auto make_pow_table () {
            constexpr size_t length = ce::log<base, std::numeric_limits<T>::max()> + 1;
            std::array<T, length> table {};
            T v = 1;
            for (T& entry : table) {
                entry = v;
                v *= base;
            }
            return table;
        }
    }

    template <size_t base, std::unsigned_integral ReturnT = uint64_t>
    [[gnu::always_inline]] constexpr ReturnT pow (uint8_t n) {
        static_assert(base <= std::numeric_limits<ReturnT>::max(), "base exceedes return type capacity");
        constexpr auto table = make_pow_table<ReturnT, base>();
        return table[n];
    }
}

