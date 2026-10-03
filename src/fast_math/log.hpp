#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <gsl/util>
#include <limits>
#include <concepts>

#include "../helper/ce.hpp"


namespace fast_math {

    namespace _detail {

        template<std::unsigned_integral T, T base>
        struct ClampedNextPowMinusOneTable {
        private:
            static constexpr size_t length = ce::log<base, std::numeric_limits<T>::max()> + 1;

            T data[length];
        public:
            consteval ClampedNextPowMinusOneTable() {
                T v = base;
                for (size_t i = 0; i < length - 1; ++i) {
                    data[i] = v - 1;
                    v *= base;
                }
                data[length - 1] = std::numeric_limits<T>::max();
            }

            [[nodiscard]] constexpr T operator [] (const uint32_t i) const { return data[i]; }
        };
    }

    template <size_t base, std::unsigned_integral T>
    constexpr uint32_t log (const T value) {
        assert(value != 0);

        using limit_t = std::numeric_limits<T>;
        static_assert(base <= limit_t::max(), "max input smaller then base");

        constexpr uint32_t max_log2 = limit_t::digits - 1;
        const uint32_t log2 = max_log2 - gsl::narrow_cast<uint32_t>(std::countl_zero(value));
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
            constexpr double base_log2 = ce::log2f<ce::Double{base}>;
            constexpr uint32_t s = std::numeric_limits<uint32_t>::digits / 2;
            constexpr uint32_t d = uint32_t{1} << s;
            constexpr uint32_t m = gsl::narrow_cast<uint32_t>(d / base_log2);

            constexpr double worst_estimate = max_log2 * (static_cast<double>(m) / d);
            constexpr double worst_estimate_true = max_log2 * (1.0 / base_log2);
            constexpr double worst_estimate_error = worst_estimate - worst_estimate_true;
            static_assert(worst_estimate_error <= 0.0 && worst_estimate_error >= -1.0);

            static_assert(m <= std::numeric_limits<uint32_t>::max() / max_log2, "Arithmetic operation would overflow");
            const uint32_t estimate = (log2 * m) >> s;

            constexpr _detail::ClampedNextPowMinusOneTable<T, base> next_pow_minus_one_table;
            return estimate + (value > next_pow_minus_one_table[estimate]);
        }
    }

    // constexpr auto unnnnn = log_unsafe<10>(0u);
}
