#pragma once

#include <cstddef>
#include <string_view>

inline bool string_view_equal (const std::string_view& a, const std::string_view& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

inline bool string_view_equal (const std::string_view& a, const char* b_data, size_t b_len) {
    if (a.size() != b_len) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] != b_data[i]) return false;
    }
    return true;
}

inline bool string_view_equal (const std::string_view& a, const char* b_begin, const char* b_end) {
    size_t b_len = b_end - b_begin;
    if (a.size() != b_len) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] != b_begin[i]) return false;
    }
    return true;
}