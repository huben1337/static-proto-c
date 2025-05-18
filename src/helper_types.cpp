#pragma once

#include <concepts>
#include <type_traits>

template<typename T>
inline constexpr bool is_char_ptr_v = std::is_same_v<T, char*> || std::is_same_v<T, const char*>;
template<typename T>
concept CharPtr = is_char_ptr_v<T>;

template<typename T>
concept Integral = std::is_integral_v<T>;

template<typename T>
inline constexpr bool is_unsigned_integral_v = std::is_integral_v<T> && std::is_unsigned_v<T>;
template<typename T>
concept UnsignedIntegral = is_unsigned_integral_v<T>;

template<typename T>
inline constexpr bool is_signed_integral_v = std::is_integral_v<T> && std::is_signed_v<T>;
template<typename T>
concept SignedIntegral = is_signed_integral_v<T>;

template <typename T>
constexpr std::size_t sizeof_v = std::is_empty_v<T> ? 0 : sizeof(T);

class Empty {};

namespace estd {

template <class __Fn, class __Ret, class... __Args>
concept invocable_r = requires (__Fn&& __fn, __Args&&... __args) {
    { __fn(std::forward<__Args>(__args)...) } -> std::same_as<__Ret>;
};
}

namespace std {
    using ::estd::invocable_r;
}

template <typename T, typename U>
constexpr T __c_cast__ (U&& value) { return (T)std::forward<U>(value); }
#define c_cast __c_cast__