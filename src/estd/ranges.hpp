#pragma once

#include <concepts>

namespace estd {
    template <std::integral T>
    struct integral_range {
        constexpr integral_range () = default;
        constexpr integral_range (T from, T to) : from(from), to(to) {}

        T from;
        T to;

        struct iterator {
            T pos;
            const iterator& operator ++ () { ++pos; return *this; }
            bool operator == (const iterator& other) const { return pos == other.pos; }
            T& operator * () { return pos; }
        };

        [[nodiscard]] iterator begin () const { return {from}; }
        [[nodiscard]] iterator end () const { return {to}; }
    };
}