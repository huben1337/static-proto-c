#pragma once

#include <array>
#include <cstddef>
#include <utility>

template <typename T, size_t N>
constexpr std::array<T, N> to_array(T (&data)[N]) {
    constexpr auto _to_array = []<size_t... Indices>(T (&data)[N], std::index_sequence<Indices...>)->std::array<T, N> {
        return {data[Indices]...};
    };
    return _to_array(data, std::make_index_sequence<N>{});
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

template <typename F, auto Value>
concept CallableWithValueTemplate = requires(F func) {
    func.template operator()<Value>();
};

template <typename F, auto Value, typename... Args>
concept CallableWithValueTemplateAndArgs = requires(F func, Args... args) {
    func.template operator()<Value>(args...);
};

template <typename T, T... Values, typename F>
requires (CallableWithValueTemplate<F, Values> && ...)
constexpr void for_(F&& func) {
    (func.template operator()<Values>(), ...);
}
template <typename T, T... Values, typename F>
requires (CallableWithValueTemplate<F, Values> && ...)
constexpr void for_(F&& func, const std::integer_sequence<T, Values...>) {
    (func.template operator()<Values>(), ...);
}

template<std::size_t N, std::size_t... Seq>
constexpr std::index_sequence<N + Seq ...> add(std::index_sequence<Seq...>) { return {}; }

template<std::size_t Min, std::size_t Max>
using make_index_range = decltype(add<Min>(std::make_index_sequence<Max-Min>()));

template <size_t base, size_t value>
constexpr size_t uint_log = value < base ? 0 : 1 + uint_log<base, value / base>;

template <size_t value>
constexpr size_t uint_log10 = uint_log<10, value>;

template <size_t value>
constexpr size_t uint_log2 = uint_log<2, value>;