#pragma once

#include <cstddef>
#include <concepts>
#include <type_traits>
#include <utility>


namespace estd {

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

    namespace _detail {
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
        _detail::check_casting_const_correctness_<To, From>();
        return reinterpret_cast<To*>(from);
    }

    template <typename To, typename From>
    [[nodiscard, gnu::always_inline]] constexpr To sibling_cast (From&& from) {
        static_assert(std::is_layout_compatible_v<std::remove_cvref_t<From>, std::remove_cvref_t<To>>,
            "Siblings must be layout compatible");
        if constexpr (std::is_lvalue_reference_v<To>) {
            static_assert(std::is_lvalue_reference_v<From>, "Can not cast from rvalue to lvalue");
            _detail::check_casting_const_correctness_<To, From>();
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
    struct constant {
        static constexpr auto value = v;
    };

    template <size_t N, typename... T>
    using nth_t = std::_Nth_type<N, T...>::type;

    template <size_t N, auto... v>
    constexpr auto nth_v = std::_Nth_type<N, constant<v>...>::type::value;

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

        template <template <typename> typename MappedType>
        using map = variadic_t<typename MappedType<T>::type...>;

        template <template <typename...> typename Template>
        using apply = Template<T...>;

        template <size_t N>
        using nth_t = nth_t<N, T...>;

        static constexpr size_t size = sizeof...(T);

        template <typename... U, typename F>
        static constexpr void foreach (F&& lambda, U&... args) {
            (std::forward<F>(lambda).template operator()<T>(args...), ...);
        }
    };


    template<typename... T>
    struct reverse_variadic_t;

    template<>
    struct reverse_variadic_t<> {
        using type = variadic_t<>;
    };

    template<typename First, typename... Rest>
    struct reverse_variadic_t<First, Rest...> {
        using type = reverse_variadic_t<Rest...>::type::template append<First>;
    };

    template<typename... T>
    using reverse_variadic_t_t = reverse_variadic_t<T...>::type;


    template <auto... v>
    using transform_to_varidadic_t = variadic_t<constant<v>...>;
    

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

        template <template <auto> typename MappedValue>
        using map = variadic_v<MappedValue<v>::value...>;

        template <template <auto...> typename Template>
        using apply = Template<v...>;
        
        template <size_t N>
        static constexpr auto nth_v = estd::nth_v<N, v...>;

        static constexpr size_t size = sizeof...(v);

        template <typename... U, typename F>
        static constexpr void foreach (F&& lambda, U&... args) {
            (std::forward<F>(lambda).template operator()<v>(args...), ...);
        }
    };


    template<auto... v>
    struct reverse_variadic_v;

    template<>
    struct reverse_variadic_v<> {
        using type = variadic_v<>;
    };

    template<auto first, auto... rest>
    struct reverse_variadic_v<first, rest...> {
        using type = reverse_variadic_v<rest...>::type::template append<first>;
    };

    template<auto... v>
    using reverse_variadic_v_t = reverse_variadic_v<v...>::type;

    namespace _detail {
        template <auto lhs, auto rhs>
        struct values_equal {
            static constexpr bool value = lhs == rhs;
        };
    }

    template <template <auto, auto> typename AreSame = _detail::values_equal>
    struct are_distinct_variadic_v {
        template<auto... v>
        struct check;

        template<>
        struct check<> {
            static constexpr bool value = true;
        };

        template<auto first, auto... rest>
        struct check<first, rest...> {
            static constexpr bool value = (!AreSame<first, rest>::value && ...) && check<rest...>::value;
        };
    };

    template <typename... T>
    using transform_to_varidadic_v = variadic_v<T::value...>;


    namespace {
        template<std::integral T, T N, T... seq>
        consteval variadic_v<N + seq ...> _make_integer_range (std::integer_sequence<T, seq...> /*unused*/) { return {}; }
    }

    template<typename T, T min, T max>
    using make_integer_range = decltype(_make_integer_range<T, min>(std::make_integer_sequence<T, max - min>{}));

    template<size_t min, size_t max>
    using make_index_range = make_integer_range<size_t, min, max>;

    template<typename T, T max>
    using make_integer_sequence = make_integer_range<T, 0, max>;

    template<size_t max>
    using make_index_sequence = make_integer_sequence<size_t, max>;

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
            static_assert(false, "TargetT not found in variadic type pack");
        };

        template <size_t N, typename TargetT, typename FirstT>
        requires (std::is_same_v<TargetT, FirstT>)
        struct variadic_type_index_<N, TargetT, FirstT> : std::integral_constant<size_t, N> {};
    }

    template <typename TargetT, typename... PossibleT>
    using variadic_type_index = variadic_type_index_<0, TargetT, PossibleT...>;

    template <typename TargetT, typename... PossibleT>
    constexpr size_t variadic_type_index_v = variadic_type_index<TargetT, PossibleT...>::value;
    
    struct discouraged_annotation {
    private:
        consteval explicit discouraged_annotation () = default;
    public:
        static const discouraged_annotation value;
    };

    constexpr discouraged_annotation discouraged_annotation::value {};

    constexpr discouraged_annotation discouraged = discouraged_annotation::value;
}
