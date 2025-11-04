#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>


namespace estd {

    template <uint64_t N>
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

    template <int64_t N>
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

    template <bool condition, typename T>
    using conditional_const = std::conditional<condition, const T, T>;

    template <bool condition, typename T>
    using conditional_const_t = conditional_const<condition, T>::type;
}

