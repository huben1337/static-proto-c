#pragma once

#include <cstddef>
#include <array>
#include <string_view>
#include <utility>
#include <type_traits>
#include <concepts>
#include "../helper/ce.hpp"
#include "../estd/utility.hpp"

template<size_t N>
struct StringLiteral {
    template <size_t>
    friend class StringLiteral; // Gives + operator access to private ctor which it needs

    template <char... chars>
    consteval StringLiteral()
    : data{chars...}
    {}

private:
    template<size_t L, size_t M, size_t ...Indecies1, size_t ...Indecies2>
    consteval StringLiteral(const char (&str1)[L], const char (&str2)[M], std::index_sequence<Indecies1...> /*unused*/, std::index_sequence<Indecies2...> /*unused*/)
    : data{str1[Indecies1]..., str2[Indecies2]...}
    {}

    template<size_t M, size_t ...Indecies>
    consteval StringLiteral(const char (&str)[M], std::index_sequence<Indecies...> /*unused*/)
    : data{str[Indecies]...}
    {}

public:
    // NOLINTNEXTLINE(google-explicit-constructor)
    consteval StringLiteral(const char (&str)[N])
    : StringLiteral{str, std::make_index_sequence<N - 1>{}}
    {
        if(str[N - 1] != 0) throw; // Null termination expected
    }

    consteval explicit StringLiteral(const char c)
        : data{c, '\0'}
    {}

    char data[N];

    [[nodiscard]] consteval size_t size () const { return N - 1; }

    [[nodiscard]] consteval const char& operator [] (size_t index) const {
        return data[index];
    }

private:
    template <size_t M, size_t... Indecies>
    requires (M == N)
    [[nodiscard]] consteval bool equals (const StringLiteral<M>& other, std::index_sequence<Indecies...> /*unused*/) const {
        return ((data[Indecies] == other.data[Indecies]) && ...);
    }

public:
    template <size_t M>
    requires (M == N)
    [[nodiscard]] consteval bool operator == (const StringLiteral<M>& other) const {
        return equals(other, std::make_index_sequence<N - 1>{});
    }

    template <size_t M>
    [[nodiscard]] consteval bool operator == (const StringLiteral<M>& other) const { return false; }

    [[nodiscard]] constexpr const char* begin () const {
        return data;
    }

    [[nodiscard]] constexpr const char* end () const {
        return data + size();
    }

    template <size_t start = 0, size_t end = N - 1>
    [[nodiscard]] consteval std::string_view to_string_view () const {
        assert_range<start, end>();
        return {data + start, end - start};
    }

private:
    template <size_t ...Indecies>
    [[nodiscard]] consteval std::array<char, sizeof...(Indecies)> _to_array (std::index_sequence<Indecies...> /*unused*/) const {
        return {data[Indecies]...};
    }

public:
    template <size_t start = 0, size_t end = N - 1>
    [[nodiscard]] consteval std::array<char, N - 1> to_array () const {
        assert_range<start, end>();
        return _to_array(estd::make_index_range<start, end>{});
    }

    template <size_t start, size_t end>
    [[nodiscard]] consteval StringLiteral<end - start + 1> substring () const {
        assert_range<start, end>();
        return StringLiteral<end - start + 1>{data, estd::make_index_range<start, end>{}};
    }

    template <size_t M>
    [[nodiscard]] consteval StringLiteral<N + M - 1> operator + (const StringLiteral<M>& other) const {
        return StringLiteral<N + M - 1>{data, other.data, std::make_index_sequence<N - 1>{}, std::make_index_sequence<M - 1>{}};
    }

private:
    template <size_t start, size_t end>
    static consteval void assert_range () {
        static_assert(start < end, "start must be less than end");
        static_assert(start < N, "start must be less than string size");
        static_assert(end < N, "end must be less than string size");
    };
};

template <size_t N>
StringLiteral(const char (&str)[N]) -> StringLiteral<N>;

StringLiteral(char) -> StringLiteral<2>;

template<StringLiteral str>
consteval decltype(str) operator ""_sl () {
    return str;
}

template <typename T>
struct is_string_literal : std::false_type {};
template <size_t N>
struct is_string_literal<StringLiteral<N>> : std::true_type {};

template <typename T>
constexpr bool is_string_literal_v = is_string_literal<T>::value;

namespace string_literal {
    namespace {
        template <char c, size_t N, size_t... Indecies>
        consteval StringLiteral<N + 1> _of (std::index_sequence<Indecies...> /*unused*/) {
            return {{((void)Indecies, c)..., '\0'}};
        }

        template <std::integral T, T value>
        consteval auto _from () {
            constexpr size_t sign_size = value < 0;
            constexpr auto unsigned_value = sign_size ? -value : value;
            constexpr size_t length = ce::log10<unsigned_value> + 1 + sign_size;
            char data[length + 1];
            size_t v = unsigned_value;
            if constexpr (sign_size) {
                data[0] = '-';
            }
            for (size_t i = 0; i < length; i++) {
                data[length - i - 1] = '0' + (v % 10);
                v /= 10;
            }
            data[length] = '\0';
            return StringLiteral{data};
        }
    }

    template <char c, size_t N>
    constexpr StringLiteral<N + 1> of = _of<c, N>(std::make_index_sequence<N>{});

    template <auto value>
    constexpr auto from = _from<decltype(value), value>();
}
