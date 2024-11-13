#include <stdint.h>
#include <algorithm>

template<size_t N>
struct StringLiteral {
    constexpr StringLiteral(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }
    
    char value[N];

    constexpr char *ptr() const {
        return (char*)(value);
    }
};

/*
template <const std::string& str>
constexpr auto make_string_literal () {
    constexpr auto N = str.length();
    return StringLiteral<N>(str.c_str());
} 
*/