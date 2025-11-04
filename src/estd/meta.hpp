#pragma once

#include <type_traits>


namespace estd {

    template <template <typename...> typename TemplateT, typename... T>
    struct type_template {
        template <typename... U>
        using type = TemplateT<T..., U...>;
    };
    template <template <typename> typename TemplateT>
    struct type_template<TemplateT> {
        template <typename U>
        using type = TemplateT<U>;
    };

    template <template <typename...> typename TemplateT, typename... T>
    struct meta : type_template<TemplateT, T...> {
        template <typename... U>
        using apply = meta<TemplateT, T..., U...>;
    };

    template <typename T>
    using is_same_meta = meta<std::is_same, T>;

    template <typename T>
    using is_any_t = std::true_type;

    template <template <typename...> typename... T>
    struct is_any_of {
        template <typename U>
        using type = std::disjunction<T<U>...>;
    };
}
