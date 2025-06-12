#pragma once
#include <cstddef>
#include <string_view>
#include <utility>

#include "constexpr_helpers.cpp"

template<size_t N>
struct StringLiteral {
    template<size_t L, size_t M, size_t ...Indecies1, size_t ...Indecies2>
    consteval StringLiteral(const char (&str1)[L], const char (&str2)[M], std::index_sequence<Indecies1...>, std::index_sequence<Indecies2...>)
        : value{str1[Indecies1]..., str2[Indecies2]...}
    {}
    template<size_t M, size_t ...Indecies>
    consteval StringLiteral(const char (&str)[M], std::index_sequence<Indecies...>)
        : value{str[Indecies]...}
    {}
    consteval StringLiteral(const char (&str)[N])
        : StringLiteral(str, std::make_index_sequence<N - 1>())
    {}
    consteval StringLiteral(const char _char)
        : value{_char, '\0'}
    {}
    // constexpr StringLiteral(const std::array<char, N> value) : value(static_cast<std::array<const char, N>>(value)) {}
        
    const char value[N];
    consteval size_t size () const { return N - 1; }
    static constexpr size_t _size = N - 1;

    consteval const char *data() const {
        return value;
    }

    consteval const char *c_str() const {
        return value;
    }

    consteval const std::string_view sv () const {
        return {value, N - 1};
    }

    consteval char operator [] (size_t index) const {
        return value[index];
    }

    template <size_t start = 0, size_t end = N - 1>
    consteval std::array<char, N-1> to_array () const {
        return _to_array(make_index_range<start, end>());
    }

    private:
    template <size_t ...Indecies>
    consteval std::array<char, sizeof...(Indecies)> _to_array (std::index_sequence<Indecies...>) const {
        return {value[Indecies]...};
    }
    public:

    template <size_t start, size_t end>
    consteval StringLiteral<end - start + 1> substring () const {
        static_assert(start < end, "start must be less than end");
        static_assert(start < N, "start must be less than string size");
        static_assert(end < N, "end must be less than string size");
        return StringLiteral<end - start + 1>{value, make_index_range<start, end>()};
    }

    template <size_t M>
    consteval StringLiteral<N + M - 1> operator + (const StringLiteral<M>& other) const {
        return StringLiteral<N + M - 1>(value, other.value, std::make_index_sequence<N - 1>(), std::make_index_sequence<M - 1>());
    }
};

template <size_t N>
StringLiteral(const char (&str)[N]) -> StringLiteral<N>;

StringLiteral(char) -> StringLiteral<2>;

template<StringLiteral str>
consteval auto operator ""_sl () {
    return str;
}

template <typename T>
struct is_string_literal_t : std::false_type {};
template <size_t N>
struct is_string_literal_t<StringLiteral<N>> : std::true_type {};

template <typename T>
constexpr bool is_string_literal_v = is_string_literal_t<T>::value;

template <typename T>
concept is_string_literal = is_string_literal_v<T>;


template <char _char, size_t N, size_t... Indecies>
static consteval StringLiteral<N + 1> _string_lieral_of (std::index_sequence<Indecies...>) {
    constexpr char data[N + 1] = {((void)Indecies, _char)..., '\0'};
    return {data};
}
template <char _char, size_t N>
static consteval StringLiteral<N + 1> string_lieral_of () {
    return _string_lieral_of<_char, N>(std::make_index_sequence<N>{});
}


template <size_t value>
consteval StringLiteral<uint_log10<value> + 2> uint_to_string () {
    constexpr size_t length = uint_log10<value> + 1;
    char data[length + 1];
    size_t v = value;
    for (size_t i = 0; i < length; i++) {
        data[length - i - 1] = '0' + (v % 10);
        v /= 10;
    }
    data[length] = '\0';
    return StringLiteral{data};
}

template <typename T, T... Values>
struct PackHolder {
    template <T... OtherValues>
    consteval auto operator + (PackHolder<T, OtherValues...>) const {
        return PackHolder<T, Values..., OtherValues...>();
    }
};

template <StringLiteral sl, size_t... Indecies>
consteval PackHolder<char, sl.value[Indecies]...> _make_char_array (std::index_sequence<Indecies...>) { return {}; }

template <StringLiteral sl>
struct CharArray {
    typedef decltype(_make_char_array<sl>(std::make_index_sequence<sl._size>())) value;

    static constexpr size_t size = sl._size;
};

// template <size_t N>
// CharArray(const char (&str)[N]) -> CharArray<StringLiteral(str)>;

template <StringLiteral sl>
consteval CharArray<sl> operator ""_ca () {
    return CharArray<sl>();
}