#pragma once

#define INLINE inline

#define static_warn(msg)                                                                                \
{                                                                                                       \
    struct Warning {                                                                                    \
        [[gnu::noinline]] [[clang::noinline]] [[gnu::warning(msg)]] static constexpr void warn () {}    \
    };                                                                                                  \
    Warning::warn();                                                                                    \
}


#if defined (__MINGW32__) || defined (__MINGW64__)
#define __MINGW__ 1
#endif
