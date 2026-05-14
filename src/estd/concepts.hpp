#pragma once

#include <type_traits>
#include <concepts>
#include <utility>

namespace estd {

    template <typename FuncT, template <typename> typename RetTT, typename... ArgsT>
    concept invocable_r = RetTT<std::invoke_result_t<FuncT, ArgsT...>>::value;

    template <typename T, template <typename> typename TT>
    concept conceptify = TT<T>::value;

    template <typename T, typename U>
    concept different_from = !std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

    template<typename From, typename To>
    concept explicitly_convertible = requires {
        static_cast<To>(std::declval<From>());
    };
}