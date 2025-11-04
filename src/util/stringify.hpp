#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <tuple>
#include <utility>
#include <type_traits>
#include <concepts>

#include "./string_literal.hpp"
#include "../container/memory.hpp"
#include "../estd/meta.hpp"

namespace stringify {
    template <StringLiteral seperator, typename ...T>
    struct Stringifyable : std::tuple<T...>  {
        constexpr explicit Stringifyable (T&&... args) : std::tuple<T...>{std::forward<T>(args)...} {}
    };

    template <typename... T>
    struct is_stringifyable_t : std::false_type {};
    template <StringLiteral seperator, typename... T>
    struct is_stringifyable_t<Stringifyable<seperator, T...>> : std::true_type {};


    template <std::unsigned_integral SizeT>
    struct GeneratorBase {
        virtual SizeT get_size() const = 0;
    };

    template <typename GeneratorT>
    concept GeneratorBaseType = requires (GeneratorT generator) {
        { generator.get_size() } -> std::unsigned_integral;
    };


    template <std::unsigned_integral SizeT>
    struct Generator : GeneratorBase<SizeT> {
        virtual char* write(char*) const = 0;
    };

    template <typename GeneratorT>
    concept GeneratorType = GeneratorBaseType<GeneratorT> && requires (GeneratorT generator, char* dst) {
        { generator.write(dst) } -> std::same_as<char*>;
    };

    template <GeneratorType T>
    constexpr char* write_string (char* dst, T&& generator) {
        return generator.write(dst);
    }
    template <GeneratorType T>
    constexpr char* _write_string (char* dst, T&& generator, const Buffer& /*unused*/) {
        return write_string(dst, std::forward<T>(generator));
    }

    struct OverAllocatedGeneratorBase {
        struct WriteResult {
            char* dst;
            Buffer::index_t over_allocation;
        };
    };

    template <std::unsigned_integral SizeT>
    struct OverAllocatedGenerator : GeneratorBase<SizeT> {
        using WriteResult = OverAllocatedGeneratorBase::WriteResult;
        virtual WriteResult write (char*) const = 0;
    };

    template <typename GeneratorT>
    concept OverAllocatedGeneratorType = GeneratorBaseType<GeneratorT> && requires (GeneratorT generator, char* dst) {
        { generator.write(dst) } -> std::same_as<OverAllocatedGeneratorBase::WriteResult>;
    };
    template <typename T>
    using is_over_allocatedd_generator = std::integral_constant<bool, OverAllocatedGeneratorType<T>>;

    template <OverAllocatedGeneratorType T>
    constexpr char* write_string (char* dst, T&& generator, Buffer& buffer) {
        OverAllocatedGeneratorBase::WriteResult result = generator.write(dst);
        buffer.go_back(result.over_allocation);
        return result.dst;
    }
    template <OverAllocatedGeneratorType T>
    constexpr char* _write_string (char* dst, T&& generator, Buffer& buffer) {
        return write_string(dst, std::forward<T>(generator), buffer);
    }


    struct UnderAllocatedGeneratorBase {
        struct Allocator {
            private:
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
            Buffer& buffer;

            public:
            explicit Allocator (Buffer& buffer) : buffer(buffer) {}

            void allocate (size_t size) const {
                buffer.next_multi_byte<char>(size);
            }
        };
    };

    template <std::unsigned_integral SizeT>
    struct UnderAllocatedGenerator : GeneratorBase<SizeT> {
        using Allocator = UnderAllocatedGeneratorBase::Allocator;
        virtual char* write (char*, Allocator&&) const = 0;
    };

    template <typename GeneratorT>
    concept UnderAllocatedGeneratorType = GeneratorBaseType<GeneratorT> && requires (GeneratorT generator, char* dst, UnderAllocatedGeneratorBase::Allocator&& allocator) {
        { generator.write(dst, allocator) } -> std::same_as<char*>;
    };

    template <UnderAllocatedGeneratorType T>
    constexpr char* write_under_allocated_generator (char* dst, T&& generator, Buffer& buffer) {
        return generator.write(dst, UnderAllocatedGeneratorBase::Allocator{buffer});
    }
    template <UnderAllocatedGeneratorType T>
    constexpr char* _write_string (char* dst, T&& generator, Buffer& buffer) {
        return write_under_allocated_generator(dst, std::forward<T>(generator), buffer);
    }


    template <GeneratorBaseType T>
    constexpr size_t get_str_size (T&& generator) {
        return generator.get_size();
    }


    template <size_t N, size_t... Indices>
    constexpr void _write_string_literal (char* dst, const char (&value)[N], std::index_sequence<Indices...> /*unused*/) {
        ((dst[Indices] = value[Indices]), ...);
    }
    template <size_t N>
    constexpr char* write_string (char* dst, const char (&value)[N]) {
        _write_string_literal(dst, value, std::make_index_sequence<N>{});
        return dst + N - 1;
    }
    template<size_t N>
    constexpr char* _write_string (char* dst, const char (&value)[N], const Buffer& /*unused*/) {
        return write_string(dst, value);
    }

    template <size_t N>
    constexpr size_t get_str_size (const char (& /*unused*/)[N]) {
        return N - 1;
    }


    template <size_t N, size_t... Indices>
    constexpr char* _write_sl (char* dst, const StringLiteral<N>& value, std::index_sequence<Indices...> /*unused*/) {
        ((dst[Indices] = value.data[Indices]), ...);
        return dst;
    }
    template <size_t N>
    constexpr char* write_string (char* dst, const StringLiteral<N>& value) {
        _write_sl(dst, value, std::make_index_sequence<N>{});
        return dst + N - 1;
    }
    template <size_t N>
    constexpr char* _write_string (char* dst, const StringLiteral<N>& value, const Buffer& /*unused*/) {
        return write_string(dst, value);
    }

    template <size_t N>
    constexpr size_t get_str_size (const StringLiteral<N>& /*unused*/) {
        return N - 1;
    }


    constexpr char* write_string (char* dst, const std::string_view& value) {
        std::memcpy(dst, value.data(), value.size());
        return dst + value.size();
    }
    constexpr char* _write_string (char* dst, const std::string_view& value, const Buffer& /*unused*/) {
        return write_string(dst, value);
    }

    constexpr size_t get_str_size (const std::string_view& str) {
        return str.size();
    }


    constexpr char* write_string (char* dst, uint64_t value) {
        if (value == 0) {
            *dst = '0';
            return dst + 1;
        } else {
            const uint8_t i = fast_math::log_unsafe<10>(value);
            char* const end = dst + i + 1;
            dst += i;
            *(dst--) = static_cast<char>('0' + (value % 10));
            value /= 10;
            while (value > 0) {
                *(dst--) = static_cast<char>('0' + (value % 10));
                value /= 10;
            }
            return end;
        }
    }
    constexpr char* _write_string (char* dst, uint64_t value, const Buffer& /*unused*/) {
        return write_string(dst, value);
    }

    constexpr size_t get_str_size (uint64_t value) {
        if (value == 0) {
            return 1;
        } else {
            return fast_math::log_unsafe<10>(value) + 1;
        }
    }

    template <typename T>
    struct _needs_buffer_arg : estd::is_any_of<is_over_allocatedd_generator>::type<T> {};

    template <typename... T, StringLiteral seperator>
    struct _needs_buffer_arg<Stringifyable<seperator, T...>> : std::disjunction<_needs_buffer_arg<T>...> {};

    template <typename... T>
    struct needs_buffer_arg : std::disjunction<_needs_buffer_arg<T>...> {};

    template <StringLiteral seperator, typename ...T, size_t... Indices>
    constexpr char* _write_stringifyable_with_seperator_with_buffer (char* dst, const Stringifyable<seperator, T...>& value, Buffer& buffer, std::index_sequence<Indices...> /*unused*/) {
        ((dst = write_string(_write_string(dst, std::get<Indices>(value), buffer), seperator)), ...);
        return dst;
    };
    template <StringLiteral seperator, typename ...T>
    constexpr char* write_stringifyable_with_buffer (char* dst, const Stringifyable<seperator, T...>& value, Buffer& buffer) {
        if constexpr (sizeof...(T) == 0) {
            return dst;
        } else {
            dst = _write_stringifyable_with_seperator_with_buffer(dst, value, buffer, std::make_index_sequence<sizeof...(T) - 1>{});
            return _write_string(dst, std::get<sizeof...(T) - 1>(value), buffer);
        }
    }
    template <StringLiteral seperator, typename ...T>
    requires (needs_buffer_arg<T...>::value) 
    constexpr char* _write_string (char* dst, const Stringifyable<seperator, T...>& value, const Buffer& buffer) {
        return write_stringifyable_with_buffer(dst, value, buffer);
    }

    template <StringLiteral seperator, typename ...T, size_t... Indices>
    constexpr char* _write_stringifyable_with_seperator (char* dst, const Stringifyable<seperator, T...>& value, std::index_sequence<Indices...> /*unused*/) {
        ((dst = write_string(write_string(dst, std::get<Indices>(value)), seperator)), ...);
        return dst;
    };
    template <StringLiteral seperator, typename ...T>
    constexpr char* write_stringifyable (char* dst, const Stringifyable<seperator, T...>& value) {
        if constexpr (sizeof...(T) == 0) {
            return dst;
        } else {
            dst = _write_stringifyable_with_seperator(dst, value, std::make_index_sequence<sizeof...(T) - 1>{});
            return write_string(dst, std::get<sizeof...(T) - 1>(value));
        }
    }
    template <StringLiteral seperator, typename ...T>
    requires (!needs_buffer_arg<T...>::value)
    constexpr char* write_string (char* dst, const Stringifyable<seperator, T...>& value) {
        return write_stringifyable(dst, value);
    }
    template <StringLiteral seperator, typename ...T>
    requires (!needs_buffer_arg<T...>::value)
    constexpr char* _write_string (char* dst, const Stringifyable<seperator, T...>& value, const Buffer& /*unused*/) {
        return write_stringifyable(dst, value);
    }

    template <StringLiteral seperator, typename ...T, size_t... Indices>
    constexpr size_t _get_stringifyable_size (const Stringifyable<seperator, T...>& value, std::index_sequence<Indices...> /*unused*/) {
        if constexpr (sizeof...(T) == 0) {
            return 0;
        } else {
            return (... + get_str_size(std::get<Indices>(value)));
        }
    };

    template <StringLiteral seperator, typename ...T>
    constexpr size_t get_str_size (const Stringifyable<seperator, T...>& value) {
        return _get_stringifyable_size(value, std::make_index_sequence<sizeof...(T)>{}) + ((sizeof...(T) - 1) * seperator.size());
    }
}