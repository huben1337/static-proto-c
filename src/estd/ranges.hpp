#pragma once

#include <cassert>
#include <concepts>
#include <gsl/util>
#include <iterator>
#include <ranges>
#include <span>
#include <type_traits>

namespace estd {
    namespace detail {
        template <typename R>
        concept SizedContiguousRange =
            std::ranges::contiguous_range<R> &&
            std::ranges::sized_range<R>;
    } // namespace detail

    template <std::unsigned_integral T>
    struct integral_range_size {
        template <std::integral>
        friend struct integral_range;
    private:
        T value;
    public:
        explicit constexpr integral_range_size (T value)
            : value(value) {}
    };

    template <std::integral T>
    struct integral_range {
        using unsigned_type = std::make_unsigned_t<T>;

    private:
        T from {};
        T to {};

    public:
        consteval integral_range () = default;

        constexpr integral_range (T from, T to) : from(from), to(to) {
            assert(from <= to);
        }
        
        constexpr integral_range (T from, integral_range_size<unsigned_type> size) : from(from), to(from + size.value) {}

        struct iterator {
        private:
            T pos;
        public:
            constexpr explicit iterator (T pos) : pos(pos) {}

            constexpr iterator& operator ++ () { ++pos; return *this; }
            constexpr bool operator == (const iterator& other) const { return pos == other.pos; }
            constexpr const T& operator * () const { return pos; }
        };

        [[nodiscard]] constexpr unsigned_type size () const { return gsl::narrow_cast<unsigned_type>(to - from); }

        [[nodiscard]] constexpr integral_range_size<unsigned_type> wrapped_size () const { 
            return integral_range_size<unsigned_type>{size()};
        }

        [[nodiscard]] constexpr iterator begin () const { return iterator{from}; }
        [[nodiscard]] constexpr iterator end () const { return iterator{to}; }

    private:
        struct template_guard {};

        template <typename R>
        using iterator_from_begin_t = decltype(std::declval<R&>().begin());

    public:
        template <
            typename Iterable,
            std::same_as<template_guard> = template_guard,
            typename With = std::span<std::ranges::range_value_t<Iterable>>
        >
        [[nodiscard]] constexpr With access_subspan (Iterable& i) const {    
            using iterable_difference_t = std::ranges::range_difference_t<Iterable>;

            assert(to <= std::ranges::size(i));

            return With{
                i.begin() + gsl::narrow_cast<iterable_difference_t>(from),
                size()
            };
        }

        template <
            typename U,
            std::same_as<template_guard> = template_guard,
            typename With = std::span<U>
        >
        [[nodiscard]] constexpr With access_subspan (U* const p) const {
            assert(p != nullptr);

            return With{
                p + from,
                size()
            };
        }

        template <
            typename Iterable,
            std::same_as<template_guard> = template_guard,
            typename With = std::ranges::subrange<std::ranges::iterator_t<Iterable>>
        >
        [[nodiscard]] constexpr With access_subrange (Iterable& i) const {
            using iterable_difference_t = std::ranges::range_difference_t<Iterable>;
            
            assert(to <= std::ranges::size(i));

            auto begin = i.begin();

            return With{
                begin + gsl::narrow_cast<iterable_difference_t>(from),
                begin + gsl::narrow_cast<iterable_difference_t>(to)
            };
        }

        template <
            typename U,
            std::same_as<template_guard> = template_guard,
            typename With = std::ranges::subrange<U*>
        >
        [[nodiscard]] constexpr With access_subrange (U* const p) const {
            assert(p != nullptr);

            return With{
                p + from,
                p + to
            };
        }
    };

    template <std::integral T>
    integral_range(T from, T to) -> integral_range<T>;

    template <std::integral T, typename U>
    integral_range(T from, integral_range_size<U> size) -> integral_range<T>;
}
