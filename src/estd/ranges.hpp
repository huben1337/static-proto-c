#pragma once

#include <cassert>
#include <concepts>
#include <gsl/util>
#include <iterator>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

namespace estd {
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
        T from;
        T to;

    public:
        constexpr integral_range () = default;
        constexpr integral_range (T from, T to) : from(from), to(to) {}
        constexpr integral_range (T from, integral_range_size<unsigned_type> size) : from(from), to(from + size.value) {}



        struct iterator {
        private:
            T pos;
        public:
            constexpr explicit iterator (T pos) : pos(pos) {}

            iterator& operator ++ () { ++pos; return *this; }
            bool operator == (const iterator& other) const { return pos == other.pos; }
            const T& operator * () const { return pos; }
        };

        [[nodiscard]] unsigned_type size () const { return gsl::narrow_cast<unsigned_type>(to - from); }

        [[nodiscard]] integral_range_size<unsigned_type> wrapped_size () const { 
            return integral_range_size<unsigned_type>{size()};
        }

        [[nodiscard]] iterator begin () const { return iterator{from}; }
        [[nodiscard]] iterator end () const { return iterator{to}; }

    private:
        struct template_guard {};

        template <typename R>
        using iterator_from_begin_t = decltype(std::declval<R&>().begin());

        template <typename Iterable, typename With>
        [[nodiscard]] With access_iterable (Iterable& i) const {
            auto begin = i.begin();
            auto accessed_end = begin + to;
            assert(accessed_end <= i.end());
            return With{
                begin + from,
                accessed_end
            };
        }

        template <typename U, typename With>
        [[nodiscard]] With access_ptr (U* const p) const {
            assert(p != nullptr);
            return With{
                p + from,
                p + to
            };
        }

    public:
        template <
            typename Iterable,
            std::same_as<template_guard> = template_guard,
            typename With = std::span<std::iter_reference_t<iterator_from_begin_t<Iterable>>>
        >
        [[nodiscard]] With access_subspan (Iterable& i) const {
            return access_iterable<Iterable, With>(i);
        }

        template <
            typename U,
            std::same_as<template_guard> = template_guard,
            typename With = std::span<U>
        >
        [[nodiscard]] With access_subspan (U* const p) const {
            return access_ptr<U, With>(p);
        }

        template <
            typename Iterable,
            std::same_as<template_guard> = template_guard,
            typename With = std::ranges::subrange<iterator_from_begin_t<Iterable>>
        >
        [[nodiscard]] With access_subrange (Iterable& i) const {
            return access_iterable<Iterable, With>(i);
        }

        template <
            typename U,
            std::same_as<template_guard> = template_guard,
            typename With = std::ranges::subrange<U*>
        >
        [[nodiscard]] With access_subrange (U* const p) const {
            return access_ptr<U, With>(p);
        }
    };
}
