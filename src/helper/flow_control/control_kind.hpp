#pragma once

#include <boost/preprocessor/facilities/empty.hpp>
#include <cstdint>
#include <boost/preprocessor.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <type_traits>

namespace flow_control::detail {

template <bool has_none, bool has_continue, bool has_break, bool has_return>
struct ControlKind_;

#define NONE() NONE,
#define CONTINUE() CONTINUE,
#define BREAK() BREAK,
#define RETURN() RETURN,
#define GOTO() GOTO,

template <bool has_none, bool has_continue, bool has_break, bool has_return>
using fitting_underlying_t = std::conditional_t<
    (static_cast<int>(has_none) 
        + static_cast<int>(has_continue) 
        + static_cast<int>(has_break)
        + static_cast<int>(has_return)
) <= 2,
    bool,
    uint8_t
>;

#define CONTROL_KIND(HAS_NONE, HAS_CONTINUE, HAS_BREAK, HAS_RETURN)                                 \
template <>                                                                                         \
struct ControlKind_<HAS_NONE, HAS_CONTINUE, HAS_BREAK, HAS_RETURN> {                                \
    enum class ControlKind : fitting_underlying_t<HAS_NONE, HAS_CONTINUE, HAS_BREAK, HAS_RETURN> {  \
        BOOST_PP_IF(HAS_NONE, NONE, BOOST_PP_EMPTY)()                                               \
        BOOST_PP_IF(HAS_CONTINUE, CONTINUE, BOOST_PP_EMPTY)()                                       \
        BOOST_PP_IF(HAS_BREAK, BREAK, BOOST_PP_EMPTY)()                                             \
        BOOST_PP_IF(HAS_RETURN, RETURN, BOOST_PP_EMPTY)()                                           \
    }; \
};


#define GENERATE_CONTROL_KIND(z, n, data)   \
CONTROL_KIND(                               \
    BOOST_PP_MOD(n, 2),                     \
    BOOST_PP_MOD(BOOST_PP_DIV(n, 2), 2),    \
    BOOST_PP_MOD(BOOST_PP_DIV(n, 4), 2),    \
    BOOST_PP_MOD(BOOST_PP_DIV(n, 8), 2)     \
)

BOOST_PP_REPEAT(16, GENERATE_CONTROL_KIND, );


#undef GENERATE_CONTROL_KIND
#undef CONTROL_KIND
#undef NONE
#undef CONTINUE
#undef BREAK
#undef RETURN
#undef GOTO

}