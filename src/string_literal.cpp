#pragma once

#include <stdint.h>
#include <utility>
#include <array>

#include "constexpr_helpers.cpp"

template<size_t N>
struct StringLiteral {
    constexpr StringLiteral(const char (&str)[N])
        : value(to_array(str))
    {}
    constexpr StringLiteral(const char _char)
        : value({_char, '\0'})
    {}
    constexpr StringLiteral(const std::array<const char, N> value) : value(value) {}
    // constexpr StringLiteral(const std::array<char, N> value) : value(static_cast<std::array<const char, N>>(value)) {}
        
    const std::array<const char, N> value;
    constexpr size_t size () const { return N - 1; }

    constexpr const char *c_str() const {
        return value.data();
    }

    constexpr const char *data() const {
        return value.data();
    }

    template <size_t M>
    constexpr StringLiteral<N + M - 1> operator + (const StringLiteral<M>& other) const {
        return StringLiteral<N + M - 1>(concat(pop_back(value), other.value));
    }
};

template <typename T>
constexpr auto to_constexpr (T t) { return t; }


template<StringLiteral str>
constexpr auto operator ""_sl () {
    return str;
}
/*
template <const std::string& str>
constexpr auto make_string_literal () {
    constexpr auto N = str.length();
    return StringLiteral<N>(str.c_str());
} 
*/