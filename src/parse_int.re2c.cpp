
#pragma once
#include <limits>
#include <type_traits>
#include "lex_result.cpp"
#include "lex_error.cpp"

/*!re2c
    re2c:define:YYMARKER = YYCURSOR;
    re2c:yyfill:enable = 0;
    re2c:define:YYCTYPE = char;

    any_white_space = [ \t\r\n];
    white_space = [ \t];
*/


#define VALUE_VAR value
#define RETURN_RESULT return { YYCURSOR - 1, is_negative ? -VALUE_VAR : VALUE_VAR };
#define CHECK_RANGE                                     \
if constexpr (max == std::numeric_limits<T>::max()) {   \
    if (new_value <= VALUE_VAR) {                       \
        UNEXPECTED_INPUT("[parse_int] value overflow"); \
    } else {                                            \
        VALUE_VAR = new_value;                          \
    }                                                   \
} else {                                                \
    if (new_value > max || new_value <= VALUE_VAR) {    \
        UNEXPECTED_INPUT("[parse_int] value overflow"); \
    } else {                                            \
        VALUE_VAR = new_value;                          \
    }                                                   \
}

#define PUSH_DEC(DIGIT) { T new_value = VALUE_VAR * 10 + DIGIT;   CHECK_RANGE }
#define PUSH_BIN(DIGIT) { T new_value = (VALUE_VAR << 1) | DIGIT; CHECK_RANGE }
#define PUSH_OCT(DIGIT) { T new_value = (VALUE_VAR << 3) | DIGIT; CHECK_RANGE }
#define PUSH_HEX(DIGIT) { T new_value = (VALUE_VAR << 4) | DIGIT; CHECK_RANGE }

/**
 * \brief Parses an integer from the input string.
 *
 * This function attempts to parse an integer from the given input string
 * starting at the position pointed to by YYCURSOR. It supports parsing
 * integers in decimal, binary, octal, and hexadecimal formats. The function
 * handles optional negative signs for signed integer types.
 *
 * \tparam T The integer type to parse, which must be an integral type.
 * \param YYCURSOR Pointer to the current position in the input string.
 * \returns A LexResult containing the parsed integer value and the updated
 *          cursor position. If the input does not represent a valid integer,
 *          an error is triggered.
 */
template <typename T, const T max = std::numeric_limits<T>::max()>
requires std::is_integral_v<T>
INLINE LexResult<T> parse_int (char *YYCURSOR) {
    T VALUE_VAR = 0;
    bool is_negative = false;

    if constexpr (std::is_signed_v<T>) {
        /*!local:re2c
            "-"                { goto minus_sign; }
            "0b"               { goto bin_entry; }
            "0"                { goto oct; }
            [1-9]              { PUSH_DEC(yych - '0'); goto dec; }
            "0x"               { goto hex_entry; }
            *                  { UNEXPECTED_INPUT("expected integer literal"); }
        */
        minus_sign:
        is_negative = true;
    }

    /*!local:re2c
        "0b"               { goto bin_entry; }
        "0"                { goto oct; }
        [1-9]              { PUSH_DEC(yych - '0'); goto dec; }
        "0x"               { goto hex_entry; }
        *                  { UNEXPECTED_INPUT("expected unsigned integer literal"); }
    */
    
bin_entry:
    /*!local:re2c
        [01]        { PUSH_BIN(yych - '0'); goto bin; }
        *           { UNEXPECTED_INPUT("expected binary digit"); }
    */
bin:
    /*!local:re2c
        [01]        { PUSH_BIN(yych - '0'); goto bin; }
        *           { RETURN_RESULT; }
    */
oct:
    /*!local:re2c
        [0-7]       { PUSH_OCT(yych - '0'); goto oct; }
        *           { RETURN_RESULT; }
    */
dec:
    /*!local:re2c
        [0-9]       { PUSH_DEC(yych - '0'); goto dec; }
        *           { RETURN_RESULT; }
    */
hex_entry:
    /*!local:re2c
        [0-9]       { PUSH_HEX(yych - '0');      goto hex; }
        [a-f]       { PUSH_HEX(yych - 'a' + 10); goto hex; }
        [A-F]       { PUSH_HEX(yych - 'A' + 10); goto hex; }
        *           { UNEXPECTED_INPUT("expected hex digit"); }
    */
hex:
    /*!local:re2c
        [0-9]       { PUSH_HEX(yych - '0');      goto hex; }
        [a-f]       { PUSH_HEX(yych - 'a' + 10); goto hex; }
        [A-F]       { PUSH_HEX(yych - 'A' + 10); goto hex; }
        *           { RETURN_RESULT; }
    */
}
#undef VALUE_VAR
#undef RETURN_RESULT
#undef CHECK_RANGE
#undef PUSH_DEC
#undef PUSH_BIN
#undef PUSH_OCT
#undef PUSH_HEX