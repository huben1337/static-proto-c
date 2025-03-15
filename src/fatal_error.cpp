#pragma once

#include <cstdlib>
#include <cstdio>


#define INTERNAL_ERROR(msg, ARGS...) fprintf(stderr, msg, ##ARGS); exit(1);