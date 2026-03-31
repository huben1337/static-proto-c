#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "../helper/ce.hpp"


namespace fast_math {

    namespace _detail {
        template<std::unsigned_integral T, T base>
        struct PowTable {
        private:
            static constexpr size_t length = ce::log<base, std::numeric_limits<T>::max()> + 1;

            T data[length];
        public:
            consteval PowTable() {
                T v = 1;
                for (T& entry : data) {
                    entry = v;
                    v *= base;
                }
            }

            [[nodiscard]] constexpr T operator [] (const uint32_t i) const { return data[i]; }
        };

    }

    template <size_t base, std::unsigned_integral ReturnT = uint64_t>
    [[gnu::always_inline]] constexpr ReturnT pow (uint8_t n) {
        static_assert(base <= std::numeric_limits<ReturnT>::max(), "base exceedes return type capacity");
        constexpr _detail::PowTable<ReturnT, base> table;
        return table[n];
    }
}

