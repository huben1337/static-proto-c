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

    namespace {
        template <typename To, typename From>
        consteval void check_casting_const_correctness_ () {
            if constexpr (std::is_const_v<std::remove_reference_t<From>>) {
                static_assert(std::is_const_v<std::remove_reference_t<To>>, "Can't cast away const");
            }
        }
    }

    template <typename To, typename From>
    [[nodiscard, gnu::always_inline]] constexpr To* ptr_cast (From* from) {
        static_assert(alignof(From) >= alignof(To), "Can't cast pointer to type of lower alignment");
        check_casting_const_correctness_<To, From>();
        return reinterpret_cast<To*>(from);
    }

    template <typename To, typename From>
    [[nodiscard, gnu::always_inline]] constexpr To sibling_cast (From&& from) {
        static_assert(std::is_layout_compatible_v<std::remove_cvref_t<From>, std::remove_cvref_t<To>>,
            "Siblings must be layout compatible");
        if constexpr (std::is_lvalue_reference_v<To>) {
            static_assert(std::is_lvalue_reference_v<From>, "Can not cast from rvalue to lvalue");
            check_casting_const_correctness_<To, From>();
        } else {
            if constexpr (!std::is_trivially_copyable_v<std::remove_reference_t<From>>) {
                static_assert(!std::is_reference_v<From>,
                    "Do not use sibling_cast to cast from lvalue to rvalue use std::move instead");
            }
            static_assert(!std::is_const_v<From>, "Do not cast from const rvalues!");
            static_assert(!std::is_const_v<To>, "Do not cast to const rvalues!");
        }
        return reinterpret_cast<To>(from);
    }

    template <auto v>
    struct value_t {
        static constexpr auto value = v;
    };

    template <size_t N, typename... T>
    using nth_t = std::_Nth_type<N, T...>::type;

    template <size_t N, auto... v>
    constexpr auto nth_v = std::_Nth_type<N, value_t<v>...>::type::value;

    template <auto first, auto... rest>
    constexpr bool are_distinct_v = ((first != rest) && ...) && are_distinct_v<rest...>;

    template <auto v>
    constexpr bool are_distinct_v<v> = true;

    template <typename... T>
    struct variadic_t {
    private:
        template <typename... U>
        [[nodiscard]] variadic_t<T..., U...> static consteval append_variadic_ (variadic_t<U...> /*unused*/) { return {}; }
    public:

        template <typename Other>
        using append_variadic = decltype(append_variadic_(Other{}));

        template <typename... U>
        using append = variadic_t<T..., U...>;

        template <size_t N>
        using nth_t = estd::nth_t<N, T...>;
    };

    template <auto... v>
    struct variadic_v {
    private:
        template <auto... w>
        [[nodiscard]] variadic_v<v..., w...> static consteval append_variadic_ (variadic_v<w...> /*unused*/) { return {}; }
    public:

        template <typename Other>
        using append_variadic = decltype(append_variadic_(Other{}));

        template <auto... w>
        using append = variadic_v<v..., w...>;

        template <size_t N>
        static constexpr auto nth_v = estd::nth_v<N, v...>;
    };

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
