#pragma once

#include <utility>

template <char C>
struct Char {
    static constexpr char value = C;
};

template<size_t N>
struct StringLiteral {
    template<size_t L, size_t M, size_t ...Indecies1, size_t ...Indecies2>
    constexpr StringLiteral(const char (&str1)[L], const char (&str2)[M], std::index_sequence<Indecies1...>, std::index_sequence<Indecies2...>)
        : value{str1[Indecies1]..., str2[Indecies2]...}
    {}
    template<size_t ...Indecies>
    constexpr StringLiteral(const char (&str)[N], std::index_sequence<Indecies...>)
        : value{str[Indecies]...}
    {}
    constexpr StringLiteral(const char (&str)[N])
        : StringLiteral(str, std::make_index_sequence<N - 1>())
    {}
    constexpr StringLiteral(const char _char)
        : value{_char, '\0'}
    {}
    // constexpr StringLiteral(const std::array<char, N> value) : value(static_cast<std::array<const char, N>>(value)) {}
        
    const char value[N];
    constexpr size_t size () const { return N - 1; }

    constexpr const char *c_str() const {
        return value;
    }

    constexpr const char *data() const {
        return value;
    }

    template <size_t M>
    constexpr StringLiteral<N + M - 1> operator + (const StringLiteral<M>& other) const {
        return StringLiteral<N + M - 1>(value, other.value, std::make_index_sequence<N - 1>(), std::make_index_sequence<M - 1>());
    }
};

template <size_t N>
StringLiteral(const char (&str)[N]) -> StringLiteral<N>;

StringLiteral(char) -> StringLiteral<2>;

template<StringLiteral str>
constexpr auto operator ""_sl () {
    return str;
}
