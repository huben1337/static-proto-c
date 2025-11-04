#pragma once

#include <cstddef>
#include <cstdint>
#include <bit>
#include <numbers>
#include <utility>
#include <gsl/util>


namespace ce {

    // Facbook Infer bug workaround
    template <typename T>
    struct Wrapped {
        consteval explicit Wrapped (T value) : value(value) {}
        T value;
    };

    using LongDouble = Wrapped<long double>;
    using Double = Wrapped<double>;
    using Float = Wrapped<float>;


    template <typename T, T... Values, typename F>
    constexpr void for_(F&& lambda) {
        (std::forward<F>(lambda).template operator()<Values>(), ...);
    }
    template <typename T, T... Values, typename F>
    constexpr void for_(F&& lambda, std::integer_sequence<T, Values...> /*unused*/) {
        (std::forward<F>(lambda).template operator()<Values>(), ...);
    }

    namespace {

        template <size_t base, size_t value>
        consteval size_t _log ();

        template <size_t base, size_t exponent>
        consteval size_t _pow ();
    }

    template <size_t base, size_t value>
    constexpr size_t log = _log<base, value>();

    template <size_t value>
    constexpr size_t log10 = log<10, value>;

    template <size_t value>
    constexpr size_t log2 = log<2, value>;

    template <size_t base, size_t exponent>
    constexpr size_t pow = base * pow<base, exponent - 1>;

    template <size_t base>
    constexpr size_t pow<base, 0> = 1;

    namespace {

        template <size_t base, size_t value>
        consteval size_t _log () {
            if constexpr (value < base) {
                return 0;
            } else {
                return 1 + log<base, value / base>;
            }
        }

        template <size_t base, size_t exponent>
        consteval size_t _pow () {
            if constexpr (exponent == 0) {
                return 1;
            } else {
                return base * pow<base, exponent - 1>;
            }
        }

        template <size_t i>
        constexpr double _log2f_coefficient = gsl::narrow_cast<double>(
            ((i % 2 == 0) ? 1.0L : -1.0L)
            / ((i + 1) * std::numbers::ln2_v<long double>)
        );

        template <Double x>
        consteval double _log2f () {
            static_assert(x.value > 0, "log2(x) undefined for non-positive x");

            constexpr size_t double_digits = 64;
            constexpr size_t sign_digits = 1;
            constexpr size_t mantissa_digits = 52;
            constexpr size_t exponent_digits = double_digits - mantissa_digits - sign_digits;
            constexpr size_t exponent_bias = 1023;
            static_assert(exponent_bias < (size_t{1} << exponent_digits), "Bias shouldn't exceed exponent space");

            constexpr uint64_t mantissa_mask = ((uint64_t{1} << mantissa_digits) - 1);

            // Decompose x into exponent and mantissa
            constexpr uint64_t bits = std::bit_cast<uint64_t>(x.value);
            constexpr int64_t exp = ((bits >> mantissa_digits) & mantissa_mask) - exponent_bias;
            constexpr uint64_t mantissa_bits = bits & mantissa_mask;
            constexpr double mantissa = 1.0 + (double{mantissa_bits} / double{uint64_t{1} << mantissa_digits});

            // Range reduction: y = M - 1 in [0,1)
            constexpr double y = mantissa - 1.0;

            double y_pow = y;
            double log2_mantissa = 0.0;
            for_([&]<size_t i>() {
                log2_mantissa += _log2f_coefficient<i> * y_pow;
                y_pow *= y;
            }, std::make_index_sequence<13>{});

            return exp + log2_mantissa;
        }
    }

    template <Double value>
    constexpr double log2f = _log2f<value>();

    template <size_t value, size_t base>
    constexpr bool is_power_of = pow<base, log<base, value>> == value;

    template <size_t value, size_t base>
    constexpr bool is_power_of_with_non_zero_exponent = value != 1 && is_power_of<value, base>;

    template <size_t N>
    constexpr bool is_power_of_two = (N != 0) && ((N & (N - 1)) == 0);
}