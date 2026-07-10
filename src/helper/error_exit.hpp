#pragma once

#include <cstdlib>
#include <utility>

#include "../util/logger.hpp"


template<typename... Args>
[[noreturn, clang::noinline, gnu::noinline, msvc::noinline, gnu::cold]] inline void error_exit(Args&&... args) {
    console.error(std::forward<Args>(args)...);
    std::exit(1);
}