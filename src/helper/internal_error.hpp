#pragma once

#include <cstdlib>

#include "../util/logger.hpp"


#define INTERNAL_ERROR(MSG, MORE...)    \
console.error<false, MSG>(MORE);        \
std::exit(1);