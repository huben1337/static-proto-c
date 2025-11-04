#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <array>
#include <type_traits>
#include <concepts>

#include "../helper/ce.hpp"


namespace fast_math {

    namespace {

        template<std::unsigned_integral T, T base>
        consteval auto _make_next_pow_table () {
            constexpr size_t length = ce::log<base, std::numeric_limits<T>::max()> + 1;
            std::array<T, length> table {};
            T v = base;
            for (T& entry : table) {
                entry = v;
                v *= base;
            }
            return table;
        }

        #define METHOD_log2(TYPE, CLZ, CLZ_ARG_TYPE)                                                        \
        [[gnu::always_inline]] constexpr auto _log2 (TYPE x) {                                              \
            constexpr auto max_bit = std::numeric_limits< std::make_unsigned_t<CLZ_ARG_TYPE> >::digits - 1; \
            return max_bit - CLZ(x);                                                                        \
        }


        METHOD_log2(unsigned long long, __builtin_clzll, unsigned long long)
        METHOD_log2(unsigned long,      __builtin_clzl , unsigned long     )
        METHOD_log2(unsigned int,       __builtin_clz  , int               )
        METHOD_log2(unsigned short,     __builtin_clz  , int               )
        METHOD_log2(unsigned char,      __builtin_clz  , int               )

        #undef METHOD_log2
    }

    template <size_t base, std::unsigned_integral T>
    constexpr uint8_t log_unsafe (T value) {
        static_assert(base <= std::numeric_limits<T>::max(), "max input smaller then base");
        uint8_t log2 = _log2(value);
        if constexpr (base == 2) {
            return log2;
        } else if constexpr (ce::is_power_of_two<base>) {
            constexpr size_t base_log2 = ce::log2<base>;
            if constexpr (ce::is_power_of_two<base_log2>) {
                return log2 >> ce::log2<base_log2>;
            } else {
                return log2 / base_log2;
            }
        } else {
            constexpr uint8_t s = 8;
            constexpr size_t d = 1 << s;
            static_assert(d > std::numeric_limits<T>::digits - 1, "Error from compentsation ratio would be to big");
            constexpr uint8_t m = static_cast<uint8_t>(d / ce::log2f<ce::Double{base}>);
            uint8_t estimate = static_cast<uint16_t>(log2 * m) >> s;
            constexpr auto next_pow_table = _make_next_pow_table<T, base>();
            return estimate + (value >= next_pow_table[estimate]);
        }
    }
}