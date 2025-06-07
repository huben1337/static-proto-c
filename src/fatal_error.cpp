#pragma once

#include <corecrt.h>
#include <cstddef>
#include <cstdlib>

#define INTERNAL_ERROR(MSG, MORE...) logger::error<false, MSG>(MORE); exit(1);