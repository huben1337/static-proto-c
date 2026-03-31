#pragma once

#include <cstddef>
#include <cstdint>
#include <gsl/util>
#include <limits>
#include <type_traits>
#include <concepts>

#include "../helper/ce.hpp"


namespace fast_math {

    namespace _detail {

        template<std::unsigned_integral T, T base>
        struct NextPowTable {
        private:
            static constexpr size_t length = ce::log<base, std::numeric_limits<T>::max()> + 1;

            T data[length];
        public:
            consteval NextPowTable() {
                T v = base;
                for (T& entry : data) {
                    entry = v;
                    v *= base;
                }
            }

            [[nodiscard]] constexpr T operator [] (const uint32_t i) const { return data[i]; }
        };

        #define METHOD_log2(TYPE, CLZ, CLZ_ARG_TYPE)                                                            \
        [[gnu::always_inline]] constexpr uint32_t _log2 (TYPE x) {                                              \
            constexpr uint32_t max_bit = std::numeric_limits< std::make_unsigned_t<CLZ_ARG_TYPE> >::digits - 1; \
            return max_bit - gsl::narrow_cast<uint32_t>(CLZ(x));                                                \
        }


        METHOD_log2(unsigned long long, __builtin_clzll, unsigned long long)
        METHOD_log2(unsigned long,      __builtin_clzl , unsigned long     )
        METHOD_log2(unsigned int,       __builtin_clz  , int               )
        METHOD_log2(unsigned short,     __builtin_clz  , int               )
        METHOD_log2(unsigned char,      __builtin_clz  , int               )

        #undef METHOD_log2
    }

    template <size_t base, std::unsigned_integral T>
    constexpr uint32_t log_unsafe (T value) {
        static_assert(base <= std::numeric_limits<T>::max(), "max input smaller then base");
        uint32_t log2 = _detail::_log2(value);
        if constexpr (base == 2) {
            return log2;
        } else if constexpr (ce::is_power_of_two<base>) {
            constexpr uint32_t base_log2 = ce::log2<base>;
            if constexpr (ce::is_power_of_two<base_log2>) {
                return log2 >> ce::log2<base_log2>;
            } else {
                return log2 / base_log2;
            }
        } else {
            constexpr size_t s = 8;
            constexpr size_t d = 1 << s;
            static_assert(d > std::numeric_limits<T>::digits - 1, "Error from compentsation ratio would be to big");
            constexpr uint32_t m = d / ce::log2f<ce::Double{base}>;
            uint32_t estimate = (log2 * m) >> s;
            constexpr _detail::NextPowTable<T, base> next_pow_table;
            return estimate + (value >= next_pow_table[estimate]);
        }
    }
}
