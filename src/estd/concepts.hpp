#pragma once

#include <type_traits>


namespace estd {

    template <typename FuncT, template <typename> typename RetTT, typename... ArgsT>
    concept invocable_r = RetTT<std::invoke_result_t<FuncT, ArgsT...>>::value;

    template <typename T, template <typename> typename TT>
    concept conceptify = TT<T>::value;
}