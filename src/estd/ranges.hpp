#pragma once

#include <concepts>
#include <gsl/util>
#include <type_traits>

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
    };
}