#pragma once

#define INLINE inline

#define static_warn(msg)                                        \
{                                                               \
    struct Warning {                                            \
        [[gnu::warning(msg)]] static constexpr void warn () {}  \
    };                                                          \
    Warning::warn();                                            \
}