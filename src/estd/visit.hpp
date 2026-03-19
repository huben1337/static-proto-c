#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

#include "../estd/type_traits.hpp"
#include "../estd/utility.hpp"
#include "../estd/concepts.hpp"


namespace estd {

    struct UnreachableFallbackHandler {
        consteval UnreachableFallbackHandler () = default;
        
        [[noreturn]] constexpr void operator () () const {
            std::unreachable();
        }
    };

    struct IdentityMapper {
        template <typename T>
        using apply = T;
    };

    struct ValueEqualityComparer {
        template <auto v>
        [[nodiscard, clang::always_inline, gnu::always_inline]] static constexpr bool matches (const auto& self) {
            return self == v;
        }
    };

    struct UnboxConstantMapper {
        template <typename T>
        static constexpr auto apply = T::value;
    };

    namespace _detail {
        template <typename Mapper, typename T, typename = void>
        struct mapper_maps_to_type : std::false_type {};

        template <typename Mapper, typename T>
        struct mapper_maps_to_type<
            Mapper,
            T,
            std::void_t<typename Mapper::template apply<T>>
        > : std::true_type {};


        template <typename Mapper, typename T>
        concept mapper_maps_to_type_ = requires {
            typename Mapper::template apply<T>;
        };
        
        
        template <typename Result, typename Mapper, typename Variant, typename Visitor, typename... Args>
        [[nodiscard]] constexpr Result dispatch_visitor (Visitor&& visitor, Args&&... args) {
            if constexpr (mapper_maps_to_type<Mapper, Variant>::value) {
                return std::forward<Visitor>(visitor).template operator()<typename Mapper::template apply<Variant>>(std::forward<Args>(args)...);
            } else {
                return std::forward<Visitor>(visitor).template operator()<Mapper::template apply<Variant>>(std::forward<Args>(args)...);
            }
        }

        template <typename Comparer, typename Mapper, typename Variant>
        [[nodiscard, clang::always_inline, gnu::always_inline]] constexpr bool variant_matches (const auto& self) {
            if constexpr (mapper_maps_to_type<Mapper, Variant>::value) {
                return Comparer::template matches<typename Mapper::template apply<Variant>>(self);
            } else {
                return Comparer::template matches<Mapper::template apply<Variant>>(self);
            }
        }


        template <
            estd::different_from<void> Result,
            typename Comparer,
            typename Mapper = IdentityMapper,
            typename... Variants,
            typename Visitor,
            typename FallbackHandler = UnreachableFallbackHandler,
            typename... Args
        >
        [[nodiscard]] constexpr Result table_visit (
            const auto& self,
            estd::variadic_t<Variants...> /*unused*/,
            Visitor&& visitor,
            FallbackHandler&& fallback_handler = {},
            Args&&... args
        ) {
            constexpr std::array visitor_dispatcher_table {
                dispatch_visitor<Result, Mapper, Variants, Visitor, Args...>
                ...
            };
            using idx_t = estd::fitting_uint_t<sizeof...(Variants)>;
            idx_t idx = -1;
            const bool matched = (
                (variant_matches<Comparer, Mapper, Variants>(self)
                    ? ((idx = estd::variadic_type_index_v<Variants, Variants...>), true)
                    : false) ||
                ...
            );

            if (matched) {
                return visitor_dispatcher_table[idx](std::forward<Visitor>(visitor), std::forward<Args>(args)...);
            }
            if constexpr (std::is_same_v<void, decltype(std::declval<FallbackHandler>()())>) {
                std::forward<FallbackHandler>(fallback_handler)();
            } else {
                return std::forward<FallbackHandler>(fallback_handler)();
            }
        }

        template <
            estd::different_from<void> Result,
            typename Comparer = ValueEqualityComparer,
            typename Mapper = UnboxConstantMapper,
            auto... variants,
            typename Visitor,
            typename FallbackHandler = UnreachableFallbackHandler,
            typename... Args
        >
        [[nodiscard]] constexpr Result table_visit (
            const auto& self,
            estd::variadic_v<variants...> /*unused*/,
            Visitor&& visitor,
            FallbackHandler&& fallback_handler = {},
            Args&&... args
        ) {
            return table_visit<Result, Comparer, Mapper, estd::constant<variants>...>
                (self, {}, std::forward<Visitor>(visitor), std::forward<FallbackHandler>(fallback_handler), std::forward<Args>(args)...);
        }
    }


    namespace _detail {
        template <
            estd::different_from<void> Result,
            typename Comparer,
            typename Mapper,
            typename FirstVariant,
            typename... RestVariants,
            typename Visitor,
            typename FallbackHandler,
            typename... Args
        >
        [[nodiscard, clang::always_inline, gnu::always_inline]] constexpr Result visit_ (
            const auto& self,
            Visitor&& visitor,
            FallbackHandler&& fallback_handler,
            Args&&... args
        ) {
            if (variant_matches<Comparer, Mapper, FirstVariant>(self)) {
                return dispatch_visitor<Result, Mapper, FirstVariant, Visitor, Args...>(std::forward<Visitor>(visitor), std::forward<Args>(args)...);
            }
            if constexpr (sizeof...(RestVariants) > 0) {
                return visit_<Result, Comparer, Mapper, RestVariants...>
                    (self, std::forward<Visitor>(visitor), std::forward<FallbackHandler>(fallback_handler), std::forward<Args>(args)...);
            } else {
                if constexpr (std::is_same_v<void, decltype(std::declval<FallbackHandler>()())>) {
                    std::forward<FallbackHandler>(fallback_handler)();
                } else {
                    return std::forward<FallbackHandler>(fallback_handler)();
                }
            }
        }
    }

    template <
        estd::different_from<void> Result,
        typename Comparer,
        typename Mapper = IdentityMapper,
        typename... Variants,
        typename Visitor,
        typename FallbackHandler = UnreachableFallbackHandler,
        typename... Args
    >
    [[nodiscard]] constexpr Result visit (
        const auto& self,
        estd::variadic_t<Variants...> /*unused*/,
        Visitor&& visitor,
        FallbackHandler&& fallback_handler = {},
        Args&&... args
    ) {
        return _detail::visit_<Result, Comparer, Mapper, Variants...>
            (self, std::forward<Visitor>(visitor), std::forward<FallbackHandler>(fallback_handler), std::forward<Args>(args)...);
    }

    template <
        estd::different_from<void> Result,
        typename Comparer = ValueEqualityComparer,
        typename Mapper = UnboxConstantMapper,
        auto... variants,
        typename Visitor,
        typename FallbackHandler = UnreachableFallbackHandler,
        typename... Args
    >
    [[nodiscard]] constexpr Result visit (
        const auto& self,
        estd::variadic_v<variants...> /*unused*/,
        Visitor&& visitor,
        FallbackHandler&& fallback_handler = {},
        Args&&... args
    ) {
        return _detail::visit_<Result, Comparer, Mapper, estd::constant<variants>...>
            (self, std::forward<Visitor>(visitor), std::forward<FallbackHandler>(fallback_handler), std::forward<Args>(args)...);
    }

    template <
        std::same_as<void> Result = void,
        typename Comparer,
        typename Mapper = IdentityMapper,
        typename... Variants,
        typename Visitor,
        typename FallbackHandler,
        typename... Args
        >
    constexpr void visit (
        const auto& self,
        estd::variadic_t<Variants...> /*unused*/,
        Visitor&& visitor,
        FallbackHandler&& fallback_handler = {},
        Args&&... args
    ) {
        const bool matched = (
            (_detail::variant_matches<Comparer, Mapper, Variants>(self)
                ? (_detail::dispatch_visitor<void, Mapper, Variants, Visitor, Args...>(std::forward<Visitor>(visitor), std::forward<Args>(args)...), true)
                : false) ||
            ...
        );

        if (matched) return;
        std::forward<FallbackHandler>(fallback_handler)();
    }

    template <
        std::same_as<void> Result = void,
        typename Comparer = ValueEqualityComparer,
        typename Mapper = UnboxConstantMapper,
        auto... variants,
        typename Visitor,
        typename FallbackHandler = UnreachableFallbackHandler,
        typename... Args
    >
    constexpr void visit (
        const auto& self,
        estd::variadic_v<variants...> /*unused*/,
        Visitor&& visitor,
        FallbackHandler&& fallback_handler = {},
        Args&&... args
    ) {
        return visit<void, Comparer, Mapper, estd::constant<variants>...>
            (self, {}, std::forward<Visitor>(visitor), std::forward<FallbackHandler>(fallback_handler), std::forward<Args>(args)...);
    }

} // namespace estd
