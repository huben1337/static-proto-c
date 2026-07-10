#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "concepts.hpp"


namespace estd {

    template <uintmax_t N>
    using fitting_uint_t = std::conditional_t<
        (N > UINT32_MAX),
        uint64_t,
        std::conditional_t<
            (N > UINT16_MAX),
            uint32_t,
            std::conditional_t<
                (N > UINT8_MAX),
                uint16_t,
                uint8_t
            >
        >
    >;

    template <intmax_t N>
    using fitting_int_t = std::conditional_t<
        (N < INT32_MIN || N > INT32_MAX),
        int64_t,
        std::conditional_t<
            (N < INT16_MIN || N > INT16_MAX),
            int32_t,
            std::conditional_t<
                (N < INT8_MIN || N > INT8_MAX),
                int16_t,
                int8_t
            >
        >
    >;

    template<std::integral T>
    using promoted_t = decltype(+std::declval<T>());

    template <bool condition, typename T>
    struct conditional_const {
        using type = T;
    };

    template <typename T>
    struct conditional_const<true, T> {
        using type = const T;
    };

    template <bool condition, typename T>
    using conditional_const_t = conditional_const<condition, T>::type;


    template <typename T>
    struct is_char_array : std::false_type {};
    template <size_t N>
    struct is_char_array<const char(&)[N]> : std::true_type {};

    template <typename T>
    constexpr bool is_char_array_v = is_char_array<T>::value;

    template <typename From, typename To>
    struct is_explicitly_convertible {
        static constexpr bool value = explicitly_convertible<From, To>;
    };

    template <typename From, typename To>
    constexpr bool is_explicitly_convertible_v = explicitly_convertible<From, To>;
}

