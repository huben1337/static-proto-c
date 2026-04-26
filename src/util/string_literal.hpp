#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <array>
#include <cstdio>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <type_traits>
#include "../helper/ce.hpp"
#include "../estd/utility.hpp"
#include "../estd/concepts.hpp"

namespace string_literal {
    template <typename T>
    consteval auto from_ (T provider);
}

template<size_t N>
struct StringLiteral {
    template <size_t>
    friend struct StringLiteral; // Gives + operator access to private ctor which it needs

    template <size_t... sizes>
    requires (sizeof...(sizes) > 0 && ((sizes - 1) + ...) == N)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    consteval explicit StringLiteral(const char (&...strs)[sizes]) {
        char* dest = data;
        ((
            dest = (std::copy_n(strs, sizes, dest)) - 1
        ), ...);
        dest[0] = '\0';
    }

    template<size_t... sizes>
    requires (sizeof...(sizes) > 0)
    consteval explicit StringLiteral(const StringLiteral<sizes>&... strs)
        : StringLiteral{strs.data...} {}

private:
    template<size_t L, size_t M, size_t ...Indecies1, size_t ...Indecies2>
    consteval StringLiteral(
        const char (&str1)[L],
        const char (&str2)[M],
        estd::variadic_v<Indecies1...> /*unused*/,
        estd::variadic_v<Indecies2...> /*unused*/
    )
        : data{str1[Indecies1]..., str2[Indecies2]..., '\0'} {}

    template<size_t M, size_t ...Indecies>
    consteval StringLiteral(const char (&str)[M], estd::variadic_v<Indecies...> /*unused*/)
        : data{str[Indecies]..., '\0'} {}

    template<size_t ...Indecies>
    consteval explicit StringLiteral(const char c, estd::variadic_v<Indecies...> /*unused*/)
        : data{((void)Indecies, c)..., '\0'} {}

    [[noreturn]] static void expected_null_terminated_char_array() { std::unreachable(); }
    [[noreturn]] static void index_out_of_range() { std::unreachable(); }

public:
    // NOLINTNEXTLINE(google-explicit-constructor)
    consteval StringLiteral(const char (&str)[N + 1])
        : StringLiteral{str, estd::make_index_sequence<N>{}} {
        if(str[N] != 0) {
            expected_null_terminated_char_array();
        }
    }

    consteval explicit StringLiteral(const char c) requires(N == 1)
        : data{c, '\0'} {}

    consteval StringLiteral() requires(N == 0)
        : data{'\0'} {}

    char data[N + 1];

    [[nodiscard]] consteval size_t size () const { return N; }

    template<std::unsigned_integral T>
    [[nodiscard]] consteval const char& operator [] (T index) const {
        if (index < N) {
            return data[index];
        }
        index_out_of_range();
    }

    template<std::signed_integral T>
    [[nodiscard]] consteval const char& operator [] (T index) const {
        index = index < 0 ? index + static_cast<ssize_t>(N) : index;
        if (index >= 0 && index < N) {
            return data[index];
        }
        index_out_of_range();
    }

private:
    template <size_t... Indecies>
    [[nodiscard]] consteval bool equals (const StringLiteral<N>& other, estd::variadic_v<Indecies...> /*unused*/) const {
        return ((data[Indecies] == other.data[Indecies]) && ...);
    }

public:
    [[nodiscard]] consteval bool operator == (const StringLiteral<N>& other) const {
        return equals(other, estd::make_index_sequence<N>{});
    }

    template <size_t M>
    [[nodiscard]] consteval bool operator == (const StringLiteral<M>& /*unused*/) const {
        return false;
    }

    [[nodiscard]] constexpr const char* begin () const {
        return data;
    }

    [[nodiscard]] constexpr const char* end () const {
        return data + size();
    }

    template <size_t start = 0, size_t end = N>
    [[nodiscard]] consteval std::string_view to_string_view () const {
        assert_range<start, end>();
        return {data + start, end - start};
    }

    template <size_t start = 0, size_t end = N>
    [[nodiscard]] consteval auto to_span () const {
        assert_range<start, end>();
        return std::span<const char, end - start>{data + start, end - start};
    }

    template <size_t start = 0, size_t end = N>
    [[nodiscard]] consteval std::ranges::subrange<const char*> to_subrange () const {
        assert_range<start, end>();
        return {data + start, data + end};
    }

private:
    template <size_t ...Indecies>
    [[nodiscard]] consteval std::array<char, sizeof...(Indecies)> _to_array (estd::variadic_v<Indecies...> /*unused*/) const {
        return {data[Indecies]...};
    }

public:
    template <size_t start = 0, size_t end = N>
    [[nodiscard]] consteval std::array<char, end - start> to_array () const {
        assert_range<start, end>();
        return _to_array(estd::make_index_range<start, end>{});
    }

    template <size_t start, size_t end = N>
    [[nodiscard]] consteval StringLiteral<end - start> substring () const {
        assert_range<start, end>();
        return {data, estd::make_index_range<start, end>{}};
    }

    template <size_t M>
    [[nodiscard]] consteval StringLiteral<N + M> operator + (const StringLiteral<M>& other) const {
        return {data, other.data, estd::make_index_sequence<N>{}, estd::make_index_sequence<M>{}};
    }


    static consteval StringLiteral<N> of (const char c) {
        return {c, estd::make_index_sequence<N>{}};
    }

private:
    template <size_t start, size_t end>
    static consteval void assert_range () {
        static_assert(start <= end, "start can not be higher than end");
        static_assert(start <= N, "start must be less than string size");
        static_assert(end <= N, "end must be less than string size");
    }
};

template <size_t N, size_t M = N - 1>
StringLiteral(const char (&str)[N]) -> StringLiteral<M>;

StringLiteral(char) -> StringLiteral<1>;

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

    constexpr StringLiteral<0> empty {};

    namespace _detail {
        template <std::integral T, T value>
        requires (std::is_integral_v<decltype(value)>)
        consteval auto from_integral () {
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
            data[length] = 0;
            return StringLiteral<length>{data};
        }

        template <typename T, T value>
        struct from;

        template <std::integral T, T value_>
        struct from<T, value_> {
            static constexpr auto value = from_integral<T, value_>();
        };

        template <estd::conceptify<is_string_literal> T, T value_>
        struct from<T, value_> {
            static constexpr auto value = value_;
        };

        template <auto value>
        constexpr auto from_v = from<decltype(value), value>::value;


        template <bool, auto... values>
        struct concat;

        template <auto... values>
        struct concat<false, values...>
            : concat<true, from_v<values>...> {};

        template <StringLiteral... strs>
        struct concat<true, strs...> {
            static constexpr auto value = StringLiteral<(strs.size() + ...)>{strs...};
        };
    }

    template <char c, size_t N>
    constexpr StringLiteral<N> of = StringLiteral<N>::of(c);

    template <auto value>
    constexpr auto from = _detail::from_v<value>;

    template <auto... values>
    struct concat
        :  _detail::concat<(is_string_literal_v<decltype(values)> && ...), values...> {};

    template <auto... values>
    constexpr auto concat_v = concat<values...>::value;


    template<StringLiteral v, StringLiteral prefix = StringLiteral<0>{}, StringLiteral postfix = StringLiteral<0>{}>
    struct join {
    private:
        template <typename Acc, auto... values>
        struct apply_;

        template <typename Acc, auto value>
        struct apply_<Acc, value> {
            using type = Acc::template append<value>;
        };

        template <typename Acc, auto first, auto... rest>
        struct apply_<Acc, first, rest...> {
            using type = apply_<typename Acc::template append<first, v>, rest...>::type;
        };

    public:
        template <auto... values>
        using apply = apply_<estd::variadic_v<prefix>, values...>
            ::type
            ::template append<postfix>
            ::template apply<concat>;
    };

    template <typename T>
    requires (std::is_invocable_r_v<std::string_view, T>)
    consteval auto from_ (T provider) {
        constexpr std::string_view sv = provider();
        constexpr const char* begin = sv.begin();
        constexpr size_t length = sv.length();
        constexpr size_t N = length + (begin[length - 1] == 0 ? 0 : 1);
        char data[N];
        for (size_t i = 0; i < length; i++) {
            data[i] = begin[i];
        }
        data[N - 1] = 0;
        return StringLiteral<N - 1>{data};
    }
}
