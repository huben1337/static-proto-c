#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gsl/util>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <type_traits>
#include <concepts>

#include "../fast_math/log.hpp"
#include "./string_literal.hpp"
#include "../estd/vector32.hpp"

//TODO Add support for Generators which dont report size by allowing allocation as needed

namespace stringify {
    template <StringLiteral seperator, typename ...T>
    struct Stringifyable : std::tuple<T...>  {
        constexpr explicit Stringifyable (T&&... args) : std::tuple<T...>{std::forward<T>(args)...} {}
    };

    template <typename... T>
    struct is_stringifyable_t : std::false_type {};
    template <StringLiteral seperator, typename... T>
    struct is_stringifyable_t<Stringifyable<seperator, T...>> : std::true_type {};

    struct Dst;
    struct Writer;

    template <typename T>
    concept Generator = requires {
        { std::declval<T>().get_size() } -> std::unsigned_integral;
        { std::declval<T>().write(std::declval<Dst&&>()) } -> std::same_as<Dst&&>;
    };

    template <typename T>
    concept UnderAllocatedGenerator = requires {
        { std::declval<T>().write(std::declval<Writer&&>()) } -> std::same_as<Writer&&>;
    };

    namespace detail {

    template <Generator T>
    constexpr size_t get_str_size (T&& generator) {
        return std::forward<T>(generator).get_size();
    }

    template <size_t N>
    constexpr size_t get_str_size (const char (& /*unused*/)[N]) {
        return N - 1;
    }

    template <size_t N>
    constexpr size_t get_str_size (const StringLiteral<N>& /*unused*/) {
        return N;
    }

    constexpr size_t get_str_size (const std::string_view& str) {
        return str.size();
    }

    constexpr size_t get_str_size (const uint64_t value) {
        if (value == 0) {
            return 1;
        }

        return fast_math::log_unsafe<10>(value) + 1;
    }

    template <StringLiteral seperator, typename ...T, size_t... Indices>
    requires (sizeof...(T) > 0)
    constexpr size_t get_stringifyable_size (const Stringifyable<seperator, T...>& value, std::index_sequence<Indices...> /*unused*/) {
        return (... + get_str_size(std::get<Indices>(value)));
    };

    template <StringLiteral seperator, typename ...T>
    constexpr size_t get_str_size (const Stringifyable<seperator, T...>& value) {
        if constexpr (sizeof...(T) == 0) {
            return 0;
        } else {
            return get_stringifyable_size<seperator, T...>(value, std::make_index_sequence<sizeof...(T)>{}) + ((sizeof...(T) - 1) * seperator.size());
        }
    }

    template <Generator T>
    constexpr char* write_string (char* dst, T&& generator);

    template <size_t N, size_t... Indices>
    constexpr void write_char_array (char* dst, const char (&value)[N], std::index_sequence<Indices...> /*unused*/) {
        ((dst[Indices] = value[Indices]), ...);
    }

    template <size_t N>
    constexpr char* write_string (char* dst, const char (&value)[N]) {
        // write_char_array(dst, value, std::make_index_sequence<N>{});
        std::memcpy(dst, value, N - 1);
        return dst + N - 1;
    }

    template <size_t N, size_t... Indices>
    constexpr void write_string_literal (char* dst, const StringLiteral<N>& value, std::index_sequence<Indices...> /*unused*/) {
        ((dst[Indices] = value.data[Indices]), ...);
    }

    template <size_t N>
    constexpr char* write_string (char* dst, const StringLiteral<N>& value) {
        // write_string_literal(dst, value, std::make_index_sequence<N>{});
        std::memcpy(dst, value.data, N);
        return dst + N;
    }

    constexpr char* write_string (char* dst, const std::string_view& value) {
        // console.debug("Writing string_view{data=", estd::ptr_as_integral(value.data()), ", size=", value.size(), "}");
        std::memcpy(dst, value.data(), value.size());
        return dst + value.size();
    }

    constexpr char* write_string (char* dst, uint64_t value) {
        if (value == 0) {
            *dst = '0';
            return dst + 1;
        }

        const uint32_t i = fast_math::log_unsafe<10>(value);
        char* const end = dst + i + 1;
        dst += i;
        *(dst--) = gsl::narrow_cast<char>('0' + (value % 10));
        value /= 10;
        while (value > 0) {
            *(dst--) = gsl::narrow_cast<char>('0' + (value % 10));
            value /= 10;
        }
        return end;
    }

    template <typename T>
    struct needs_allocator {
        static constexpr bool value = false;
    };

    template <UnderAllocatedGenerator T>
    struct needs_allocator<T> {
        static constexpr bool value = true;
    };

    template <typename... T, StringLiteral seperator>
    struct needs_allocator<Stringifyable<seperator, T...>> {
        static constexpr bool value = (needs_allocator<T>::value || ...);
    };

    template <typename T>
    constexpr bool needs_allocator_v = needs_allocator<T>::value;

    template <typename... T>
    constexpr bool any_needs_allocator_v = (needs_allocator_v<T> || ...);

    template <StringLiteral seperator, typename ...T, size_t... Indices>
    requires (sizeof...(T) > 1)
    constexpr char* write_stringifyable_with_seperator (char* dst, const Stringifyable<seperator, T...>& value, std::index_sequence<Indices...> /*unused*/) {
        ((dst = write_string(write_string(dst, std::get<Indices>(value)), seperator)), ...);
        return dst;
    };

    template <StringLiteral seperator, typename ...T>
    constexpr char* write_stringifyable (char* dst, const Stringifyable<seperator, T...>& value) {
        if constexpr (sizeof...(T) == 0) {
            return dst;
        } else {
            if constexpr (sizeof...(T) > 1) {
                dst = write_stringifyable_with_seperator(dst, value, std::make_index_sequence<sizeof...(T) - 1>{});
            }

            return write_string(dst, std::get<sizeof...(T) - 1>(value));
        }
    }

    template <StringLiteral seperator, typename ...T>
    requires (!any_needs_allocator_v<T...>)
    constexpr char* write_string (char* dst, const Stringifyable<seperator, T...>& value) {
        return write_stringifyable(dst, value);
    }

    } // namespace detail

    struct Dst {

    template <Generator T>
    friend constexpr char* detail::write_string (char* dst, T&& generator);

    private:
        char* dst;

    public:
        constexpr explicit Dst(char* dst) : dst(dst) {}
        
        Dst(const Dst&) = delete;
        Dst(Dst&&) = delete;

        Dst& operator=(const Dst&) = delete;
        Dst& operator=(Dst&&) = delete;

        constexpr ~Dst() = default;

        template<typename... Args>
        constexpr void write(Args&&... args) {
            ((dst = detail::write_string(dst, std::forward<Args>(args))), ...);
        }

        constexpr void write_n(char c, size_t n) {
            std::memset(dst, c, n);
            dst += n;
        }

        /* template <Generator T>
        constexpr void write (T&& generator) {
            dst = generator.write(Dst{dst}).dst;
        }

        template <size_t N>
        constexpr void write (const char (&value)[N]) {
            std::memcpy(dst, value, N - 1);
            dst = dst + N - 1;
        }

        template <size_t N>
        constexpr void write (const StringLiteral<N>& value) {
            std::memcpy(dst, value.data, N);
            dst = dst + N;
        }

        constexpr void write (const std::string_view& value) {
            std::memcpy(dst, value.data(), value.size());
            dst = dst + value.size();
        } */
    };

    template<typename... Args>
    constexpr void write_into (estd::vector32<char>& buffer, Args&&... args) {
        const size_t size = (... + detail::get_str_size(args));
        constexpr size_t max_size = std::numeric_limits<uint32_t>::max();
        if constexpr (max_size < std::numeric_limits<size_t>::max()) {
            assert(size < max_size);
        }

        const uint32_t old_size = buffer.size();
        buffer.uninitialized_resize(old_size + gsl::narrow_cast<uint32_t>(size));
        
        char* const begin = buffer.data() + old_size;
        char* dst = begin;
        ((dst = detail::write_string(dst, std::forward<Args>(args))), ...);
        assert(dst >= begin);
        if constexpr (detail::any_needs_allocator_v<std::remove_cvref_t<Args>...>) {
            buffer.uninitialized_resize(gsl::narrow_cast<uint32_t>(dst - buffer.data()));
        }
    }

    template<typename... Args>
    constexpr void write_into (std::string& str, Args&&... args) {
        const size_t size = (... + detail::get_str_size(args));
        const size_t old_size = str.size();
        str.resize(old_size + size);
        char* const begin = str.data() + old_size;
        char* dst = begin;
        ((dst = detail::write_string(dst, std::forward<Args>(args))), ...);
        assert(dst >= begin);
        if constexpr (detail::any_needs_allocator_v<std::remove_cvref_t<Args>...>) {
            str.resize(gsl::narrow_cast<size_t>(dst - str.data()));
        }
    }

    template<typename... Args>
    constexpr std::string write_to_string (Args&&... args) {
        const size_t size = (... + detail::get_str_size(args));
        std::string str (size, 0);
        char* dst = str.data();
        ((dst = detail::write_string(dst, std::forward<Args>(args))), ...);
        assert(dst >= str.data());
        assert(dst <= str.data() + str.size());
        str.resize(gsl::narrow_cast<size_t>(dst - str.data()));
        return str;
    }

    namespace detail {
        template <Generator T>
        constexpr char* write_string (char* dst, T&& generator) {
            return generator.write(Dst{dst}).dst;
        }
    }
}