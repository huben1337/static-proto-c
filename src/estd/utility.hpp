#pragma once

#include <cstddef>
#include <concepts>
#include <utility>


namespace estd {

    /* integer and index range */
    namespace {
        template<std::integral T, T N, T... seq>
        consteval std::integer_sequence<T, N + seq ...> _integer_sequence_add_all (std::integer_sequence<T, seq...> /*unused*/) { return {}; }
    }

    template<typename T, T min, T max>
    using make_integer_range = decltype(_integer_sequence_add_all<T, min>(std::make_integer_sequence<T, max - min>{}));

    template<size_t min, size_t max>
    using make_index_range = make_integer_range<size_t, min, max>;


    /* conditional forward */
    template <bool condition, typename T, typename U>
    requires (condition)
    [[gnu::always_inline]] constexpr T&& conditionally (T&& t, U&& /*unused*/) noexcept {
        return std::forward<T>(t);
    }

    template <bool condition, typename T, typename U>
    requires (!condition)
    [[gnu::always_inline]] constexpr U&& conditionally (T&& /*unused*/, U&& u) noexcept {
        return std::forward<U>(u);
    }
}
