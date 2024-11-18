#pragma once

template <typename T>
struct LexResult {
    char *cursor;
    T value;
};