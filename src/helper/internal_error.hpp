#pragma once

#include <cstdlib>

#include "../util/logger.hpp"


#define INTERNAL_ERROR(MSG, ...)        \
console.error<false, MSG>(__VA_ARGS__); \
std::exit(1);