#pragma once
#include <string>
#include "base.cpp"
#include "helper_types.cpp"

template <UnsignedIntegral T>
struct StringSection {
    const char* offset;
    T length;

    INLINE StringSection () = default;

    INLINE StringSection(const char* start, const char* end) : offset(start), length(end - start) {}
    INLINE StringSection(const char* start, T length) : offset(start), length(length) {}
};

INLINE std::string extract_string (const char *offset, const char *end) {
    return std::string(offset, end - offset);
}

INLINE std::string extract_string (const char *offset, size_t length) {
    return std::string(offset, length);
}


template <UnsignedIntegral T>
INLINE std::string extract_string (StringSection<T> string_section) {
    auto [offset, length] = string_section;
    return std::string(offset, length);
}

template <typename AEnd, typename BEnd>
requires (std::is_integral_v<AEnd> || is_char_ptr_v<AEnd>) && (std::is_integral_v<BEnd> || is_char_ptr_v<BEnd>)
INLINE bool string_section_eq(const char* a_start, AEnd a_end_or_length, const char* b_start, BEnd b_end_or_length) {
    size_t a_length = 0;
    size_t b_length = 0;
    
    if constexpr (is_char_ptr_v<AEnd>) {
        a_length = a_end_or_length - a_start;
    } else {
        a_length = a_end_or_length;
    }
    
    if constexpr (is_char_ptr_v<BEnd>) {
        b_length = b_end_or_length - b_start;
    } else {
        b_length = b_end_or_length;
    }
    

    if (a_length != b_length) return false;
    for (size_t i = 0; i < a_length; i++) {
        if (a_start[i] != b_start[i]) return false;
    }
    
    return true;
}