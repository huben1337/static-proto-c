#pragma once

#include <stdio.h>

#define INTERNAL_ERROR(msg) fprintf(stderr, msg); exit(1);