#pragma once

#include <cstddef>
#include <concepts>
#include <type_traits>
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

    template <typename To, typename From>
    [[nodiscard, gnu::always_inline]] constexpr To* ptr_cast (From* from) {
        static_assert(alignof(From) >= alignof(To), "Can't cast pointer to type of lower alignment");
        if constexpr (std::is_const_v<std::remove_reference_t<From>>) {
            static_assert(std::is_const_v<std::remove_reference_t<To>>, "Can't cast away const");
        }
        return reinterpret_cast<To*>(from);
    }

    template <size_t N, typename... T>
    using nth_type_t = std::_Nth_type<N, T...>::type;

    namespace {
        template <size_t N, typename TargetT, typename FirstT, typename... RestT>
        struct variadic_type_index_ : std::integral_constant<
            size_t,
            variadic_type_index_<N + 1, TargetT, RestT...>::value
        > {};

        template <size_t N, typename TargetT, typename FirstT, typename... RestT>
        requires (std::is_same_v<TargetT, FirstT>)
        struct variadic_type_index_<N, TargetT, FirstT, RestT...> : std::integral_constant<size_t, N> {};

        template <size_t N, typename TargetT, typename FirstT>
        struct variadic_type_index_<N, TargetT, FirstT> {
            static_assert(false, "Invalid variant type.");
        };

        template <size_t N, typename TargetT, typename FirstT>
        requires (std::is_same_v<TargetT, FirstT>)
        struct variadic_type_index_<N, TargetT, FirstT> : std::integral_constant<size_t, N> {};
    }

    template <typename TargetT, typename... PossibleT>
    using variadic_type_index = variadic_type_index_<0, TargetT, PossibleT...>;

    template <typename TargetT, typename... PossibleT>
    constexpr size_t variadic_type_index_v = variadic_type_index<TargetT, PossibleT...>::value;
}
