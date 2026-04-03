#pragma once

#define static_warn(msg)                                                                        \
{                                                                                               \
    struct Warning {                                                                            \
        [[gnu::noinline, clang::noinline, gnu::warning(msg)]] static constexpr void warn () {}  \
    };                                                                                          \
    Warning::warn();                                                                            \
}
