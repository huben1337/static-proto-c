#pragma once

#include <cstdint>
#include <array>
#include <utility>
#include "base.cpp"

template <typename T, size_t N>
constexpr std::array<T, N> to_array(T (&str)[N]) {
    constexpr auto _to_array = []<size_t... Indices>(T (&str)[N], std::index_sequence<Indices...>)->std::array<T, N> {
        return {str[Indices]...};
    };
    return _to_array(str, std::make_index_sequence<N>{});
}

template <typename T, size_t N>
constexpr std::array<T, N + 1> push_back(std::array<T, N> arr, std::remove_const_t<T> value) {
    constexpr auto _push_back = []<size_t... Indices>(std::array<T, N> arr, std::remove_const_t<T> value, std::index_sequence<Indices...>)->std::array<T, N + 1> {
        return {arr[Indices]..., value};
    };
    return _push_back(arr, value, std::make_index_sequence<N>{});
}

template <typename T, size_t N>
constexpr std::array<T, N + 1> push_front(std::array<T, N> arr, std::remove_const_t<T> value) {
    constexpr auto _push_front = []<size_t... Indices>(std::array<T, N> arr, std::remove_const_t<T> value, std::index_sequence<Indices...>)->std::array<T, N + 1> {
        return {value, arr[Indices]...};
    };
    return _push_front(arr, value, std::make_index_sequence<N>{});
}

template <typename T, size_t N>
requires (N > 0)
constexpr std::array<T, N - 1> pop_back(std::array<T, N> arr) {
    constexpr auto _pop_back = []<size_t... Indices>(std::array<T, N> arr, std::index_sequence<Indices...>)->std::array<T, N - 1> {
        return {arr[Indices]...};
    };
    return _pop_back(arr, std::make_index_sequence<N - 1>{});
}

template <typename T, size_t N>
requires (N > 0)
constexpr std::array<T, N - 1> pop_front(std::array<T, N> arr) {
    constexpr auto _pop_front = []<size_t... Indices>(std::array<T, N> arr, std::index_sequence<Indices...>)->std::array<T, N - 1> {
        return {arr[Indices + 1]...};
    };
    return _pop_front(arr, std::make_index_sequence<N - 1>{});
}

template <typename T, size_t N, size_t M>
constexpr std::array<T, N + M> concat(std::array<T, N> arr1, std::array<T, M> arr2) {
    constexpr auto _concat = []<size_t... Indices1, size_t... Indices2>(std::array<T, N> arr1, std::array<T, M> arr2, std::index_sequence<Indices1...>, std::index_sequence<Indices2...>)->std::array<T, N + M> {
        return {arr1[Indices1]..., arr2[Indices2]...};
    };
    return _concat(arr1, arr2, std::make_index_sequence<N>{}, std::make_index_sequence<M>{});
}

template <typename F, typename T, T Value>
concept CallableWithValueTemplate = requires(F func) {
    func.template operator()<Value>();
};
template <typename T, T... Values, typename F>
requires (CallableWithValueTemplate<F, T, Values> && ...)
constexpr void for_(F&& func) {
    (func.template operator()<Values>(), ...);
}
template <typename T, T... Values, typename F>
requires (CallableWithValueTemplate<F, T, Values> && ...)
constexpr void for_(F&& func, const std::integer_sequence<T, Values...>) {
    (func.template operator()<Values>(), ...);
}
