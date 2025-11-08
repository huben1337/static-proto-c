#pragma once

#define ALLOCA(TYPE, LENGTH) static_cast<TYPE*>(__builtin_alloca_with_align((LENGTH) * sizeof(TYPE), alignof(TYPE) * 8))

#define ALLOCA_SAFE(NAME, TYPE, LENGTH) TYPE* NAME;         \
if (LENGTH > 0) {                                           \
    NAME = ALLOCA(TYPE, LENGTH);                            \
} else {                                                    \
    NAME = nullptr;                                         \
}