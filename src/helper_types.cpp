#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

template<typename T>
inline constexpr bool is_char_ptr_v = std::is_same_v<T, char*> || std::is_same_v<T, const char*>;
template<typename T>
concept CharPtr = is_char_ptr_v<T>;

template <typename T>
constexpr std::size_t sizeof_v = std::is_empty_v<T> ? 0 : sizeof(T);

class Empty {};

namespace estd {

template <class __Fn, class __Ret, class... __Args>
concept invocable_r = requires (__Fn&& __fn, __Args&&... __args) {
    { __fn(std::forward<__Args>(__args)...) } -> std::same_as<__Ret>;
};

class const_copy_detail {
    template <typename T>
    static inline constexpr const T const_copy (T& value) {
        static_assert(std::is_const_v<std::remove_reference_t<T>>, "can only copy const lvalues");
        return value;
    }

    friend inline constexpr const auto const_copy (auto&& value);
};

inline constexpr const auto const_copy (auto&& value) {
    return const_copy_detail::const_copy(std::forward<decltype(value)>(value));
}

}

namespace std {
    using ::estd::invocable_r;
}

template <uint64_t N>
using fitting_uint = std::conditional_t<
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
using fitting_int = std::conditional_t<
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