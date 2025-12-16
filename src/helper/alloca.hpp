#pragma once

#define ALLOCA(TYPE, LENGTH) \
static_cast<TYPE*>(__builtin_alloca_with_align((LENGTH) * sizeof(TYPE), alignof(TYPE) * 8))

#define ALLOCA_SAFE(NAME, TYPE, LENGTH) TYPE* NAME;         \
if (size_t length = LENGTH; length > 0) {                   \
    NAME = ALLOCA(TYPE, length);                            \
} else {                                                    \
    NAME = nullptr;                                         \
}

#define ALLOCA_UNSAFE_SPAN(NAME, TYPE, LENGTH)                      \
std::span<TYPE> NAME;                                               \
{                                                                   \
    const size_t length = LENGTH;                                   \
    NAME = std::span<TYPE>{ALLOCA(TYPE, length), length};           \
}

#define ALLOCA_SAFE_SPAN(NAME, TYPE, LENGTH)                        \
std::span<TYPE> NAME;                                               \
if (const size_t length = LENGTH; length > 0) {                     \
    NAME = std::span<TYPE>{ALLOCA(TYPE, length), length};           \
}