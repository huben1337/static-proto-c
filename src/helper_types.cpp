#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

template <typename T>
constexpr size_t sizeof_v = std::is_empty_v<T> ? 0 : sizeof(T);

class Empty {};

namespace estd {

template <template <typename...> typename TemplateT, typename... T>
struct type_template {
    template <typename... U>
    using type = TemplateT<T..., U...>;
};
template <template <typename> typename TemplateT, typename T>
struct type_template<TemplateT, T> {
    using type = TemplateT<T>;
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

template <class __Fn, template <class> class __Ret, class... __Args>
concept invocable_r = requires (__Fn&& __fn, __Args&&... __args) {
    requires __Ret<decltype(__fn(std::forward<__Args>(__args)...))>::value;
};

template <bool condition, typename T>
inline constexpr T&& conditionally (T&& t, auto) noexcept requires(condition) {
    return std::forward<T>(t);
}

template <bool condition, typename T>
inline constexpr T&& conditionally (auto, T&& t) noexcept requires(!condition) {
    return std::forward<T>(t);
}




class const_copy_detail {
    template <typename T>
    static inline constexpr const T const_copy (T& value) {
        static_assert(std::is_const_v<std::remove_reference_t<T>>, "can only copy const lvalues");
        return value;
    }

    friend inline constexpr const auto const_copy (auto&& value);
};

inline constexpr const auto const_copy (auto&& value) {
    return const_copy_detail::const_copy(std::forward<decltype(value)>(value));
}

}

namespace std {
}

template <uint64_t N>
using fitting_uint = std::conditional_t<
    (N > UINT32_MAX),
    uint64_t,
    std::conditional_t<
        (N > UINT16_MAX),
        uint32_t,
        std::conditional_t<
            (N > UINT8_MAX),
            uint16_t,
            uint8_t
        >
    >
>;

template <int64_t N>
using fitting_int = std::conditional_t<
    (N < INT32_MIN || N > INT32_MAX),
    int64_t,
    std::conditional_t<
        (N < INT16_MIN || N > INT16_MAX),
        int32_t,
        std::conditional_t<
            (N < INT8_MIN || N > INT8_MAX),
            int16_t,
            int8_t
        >
    >
>;