#pragma once

#include "string_literal.cpp"
#include "lex_error.cpp"

/*!re2c
    re2c:define:YYMARKER = YYCURSOR;
    re2c:yyfill:enable = 0;
    re2c:define:YYCTYPE = char;

    any_white_space = [ \t\r\n];
    white_space = [ \t];
*/


template<char symbol>
constexpr auto lex_symbol_error = StringLiteral({'e','x','p','e', 'c', 't', 'e', 'd', ' ', 's', 'y', 'm', 'b', 'o', 'l', ':', ' ', symbol});


#define CHECK_SYMBOL                                                \
if (yych == symbol) {                                               \
    return ++YYCURSOR;                                              \
} else {                                                            \
    UNEXPECTED_INPUT(error_message.value)                           \
}

/**
 * \brief Lexes a specific symbol, allowing any amount of whitespace before it.
 *
 * This function attempts to lex a specified symbol in the input string
 * starting at the position pointed to by YYCURSOR. It skips over any amount
 * of whitespace before checking for the symbol.
 *
 * \tparam symbol The character symbol to lex.
 * \param YYCURSOR Pointer to the current position in the input.
 * \returns A pointer to the position immediately following the symbol,
 *          or triggers an error if the symbol is not found.
 */
template <char symbol, StringLiteral error_message = lex_symbol_error<symbol>>
__forceinline char *lex_symbol (char *YYCURSOR) {
    /*!local:re2c
        any_white_space* { CHECK_SYMBOL }
    */
}

/**
 * \brief Lexes a specific symbol on the same line.
 *
 * This function attempts to lex a specified symbol that is expected to appear
 * on the same line as the current position in the input. It skips over any
 * leading whitespace before checking for the symbol.
 *
 * \tparam symbol The character symbol to lex.
 * \param YYCURSOR Pointer to the current position in the input.
 * \returns A pointer to the position immediately following the symbol,
 *          or triggers an error if the symbol is not found.
 */
template <char symbol, StringLiteral error_message = lex_symbol_error<symbol>>
__forceinline char *lex_same_line_symbol (char *YYCURSOR) {
    /*!local:re2c
        white_space* { CHECK_SYMBOL }
    */
}

#undef CHECK_SYMBOL

/**
 * \brief Skips over any white space characters in the input.
 *
 * This function advances the cursor over any sequence of white space
 * characters, including spaces and tabs, in the given input.
 *
 * \param YYCURSOR Pointer to the current position in the input.
 * \returns A pointer to the position immediately following any skipped
 *          white space characters, or the same position if no white space
 *          was found.
 */
__forceinline char *skip_white_space (char *YYCURSOR) {
    /*!local:re2c
        white_space* { return YYCURSOR; }
    */
}

#define CHECK_SYMBOL                                                \
if (yych == symbol) {                                               \
    return ++YYCURSOR;                                              \
} else {                                                            \
    UNEXPECTED_INPUT(error_message.value)                           \
}

/**
 * \brief Skips over any white space characters in the input.
 *
 * This function advances the cursor over any sequence of white space
 * characters, including spaces, tabs, carriage returns, and newlines,
 * in the given input.
 *
 * \param YYCURSOR Pointer to the current position in the input.
 * \returns A pointer to the position immediately following any skipped
 *          white space characters, or the same position if no white space
 *          was found.
 */
__forceinline char *skip_any_white_space (char *YYCURSOR) {
    /*!local:re2c
        any_white_space* { return YYCURSOR; }
    */
}

__forceinline char *lex_white_space (char *YYCURSOR) {
    /*!local:re2c
        white_space+ { return YYCURSOR; }
        * { UNEXPECTED_INPUT("expected white space"); }
    */
}

__forceinline char *lex_any_white_space (char *YYCURSOR) {
    /*!local:re2c
        any_white_space+ { return YYCURSOR; }
        * { UNEXPECTED_INPUT("expected any white space"); }
    */
}