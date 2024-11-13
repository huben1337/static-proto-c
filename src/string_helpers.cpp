#include <string>

template <typename T>
requires std::is_integral_v<T> && std::is_unsigned_v<T>
struct StringSection {
    const char* offset;
    T length;

    StringSection () = default;

    StringSection(const char* start, const char* end) : offset(start), length(end - start) {}
    StringSection(const char* start, T length) : offset(start), length(length) {}
};

std::string extract_string (const char *offset, const char *end) {
    return std::string(offset, end - offset);
}

std::string extract_string (const char *offset, size_t length) {
    return std::string(offset, length);
}


template <typename T>
std::string extract_string (StringSection<T> string_section) {
    auto [offset, length] = string_section;
    return std::string(offset, length);
}
