#pragma once

#include <type_traits>
#include <utility>
#include <boost/preprocessor.hpp>

namespace thief_detail {
    template<typename T>
    union block_dtor {
        constexpr explicit block_dtor (T&& target) : target(std::move(target)) {
            static_assert(!std::is_reference_v<T>, "block_dtor needs to be constructed with rvalue reference and member must be non-reference type");
        }
        T target;
        ~block_dtor() {}
    };
}

#define THIEF_PP_EXPAND_STEAL(r, data, member) \
std::move(wrapped.target.member)

#define THIEF(TARGET_TYPE, MEMBERS...)                                  \
[](TARGET_TYPE&& target) {                                              \
    using target_t = TARGET_TYPE;                                       \
    thief_detail::block_dtor<target_t> wrapped {std::move(target)};     \
    return std::tuple{                                                  \
        BOOST_PP_SEQ_ENUM(                                              \
            BOOST_PP_SEQ_TRANSFORM(                                     \
                THIEF_PP_EXPAND_STEAL, /* placeholder */,               \
                BOOST_PP_VARIADIC_TO_SEQ(MEMBERS)                       \
            )                                                           \
        )                                                               \
    };                                                                  \
}                                                                       \

#define PREVENT_DESTRUCTION(TARGET) \
(void) thief_detail::block_dtor{std::move(TARGET)}