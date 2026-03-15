#pragma once

#include <utility>

namespace estd {

#define IDENTITY(...) __VA_ARGS__

#define BINARY_OP_IMPL(STATIC_ATTR, NAME, OP)                               \
template<typename T, typename U>                                            \
[[nodiscard]] IDENTITY(STATIC_ATTR) constexpr auto NAME(T&& lhs, U&& rhs) { \
    return std::forward<T>(lhs) OP std::forward<U>(rhs);                    \
}

#define BINARY_OP_PROVIDER(NAME, OP)   \
struct NAME {                          \
    BINARY_OP_IMPL(static, apply, OP)  \
    BINARY_OP_IMPL(, operator(), OP)   \
}

BINARY_OP_PROVIDER(add, +);
BINARY_OP_PROVIDER(substract, -);
BINARY_OP_PROVIDER(multiply, *);
BINARY_OP_PROVIDER(divide, /);

#undef BINARY_OP_PROVIDER
#undef BINARY_OP_IMPL
#undef IDENTITY

template <typename Operator, typename T, typename U>
constexpr T& compound_asign (T& lhs, U&& rhs) {
    return lhs = Operator::apply(lhs, std::forward<U>(rhs));
}

} // namespace estd