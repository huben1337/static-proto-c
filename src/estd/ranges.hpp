#pragma once

#include <cassert>
#include <concepts>
#include <gsl/pointers>
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
        T _value;

    public:
        explicit constexpr integral_range_size (T value)
            : _value(value) {}

        [[nodiscard]] constexpr const T& get() const { return _value; }
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
        
        constexpr integral_range (T from, integral_range_size<unsigned_type> size) :
            from(from),
            to(from + gsl::narrow_cast<T>(size._value))
        {}

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
        template <typename R>
        using iterator_from_begin_t = decltype(std::declval<R&>().begin());

    public:
        template <std::ranges::contiguous_range Range>
        requires (std::is_unsigned_v<T>)
        [[nodiscard]] constexpr auto access_subspan (Range& i) const {
            using range_reference_t = std::ranges::range_reference_t<Range>;
            using range_difference_t = std::ranges::range_difference_t<Range>;

            assert(to <= std::ranges::size(i));

            return std::span<std::remove_reference_t<range_reference_t>>{
                std::ranges::data(i) + gsl::narrow_cast<range_difference_t>(from),
                size()
            };
        }

        template <typename U>
        requires (std::is_unsigned_v<T>)
        [[nodiscard]] constexpr auto access_subspan (gsl::not_null<U*> const p) const {
            return std::span<U>{
                p.get() + from,
                size()
            };
        }

        template <std::ranges::contiguous_range Range>
        requires (std::is_unsigned_v<T>)
        [[nodiscard]] constexpr auto access_subrange (Range& i) const {
            using range_difference_t = std::ranges::range_difference_t<Range>;
            
            assert(to <= std::ranges::size(i));

            auto begin = std::ranges::begin(i);

            return std::ranges::subrange<std::ranges::iterator_t<Range>>{
                begin + gsl::narrow_cast<range_difference_t>(from),
                begin + gsl::narrow_cast<range_difference_t>(to)
            };
        }

        template <typename U>
        requires (std::is_unsigned_v<T>)
        [[nodiscard]] constexpr auto access_subrange (gsl::not_null<U*> const p) const {

            return std::ranges::subrange<U*>{
                p.get() + from,
                p.get() + to
            };
        }
    };

    template <std::integral T>
    integral_range(T from, T to) -> integral_range<T>;

    template <std::integral T, typename U>
    integral_range(T from, integral_range_size<U> size) -> integral_range<T>;
}
