#pragma once
#include <type_traits>

template<typename T>
inline constexpr bool is_char_ptr_v = std::is_same_v<T, char*> || std::is_same_v<T, const char*>;
template<typename T>
concept CharPtr = is_char_ptr_v<T>;

template<typename T>
inline constexpr bool is_unsigned_integral_v = std::is_integral_v<T> && std::is_unsigned_v<T>;
template<typename T>
concept UnsignedIntegral = is_unsigned_integral_v<T>;