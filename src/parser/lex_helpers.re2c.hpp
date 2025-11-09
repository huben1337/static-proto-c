#pragma once

#include <string_view>
#include <utility>
#include "../util/string_literal.hpp"
#include "./lex_error.hpp"


namespace lexer {

/*!re2c
    re2c:yyfill:enable = 0;
    re2c:define:YYCTYPE = char;

    re2c:api = generic;
    re2c:api:style = free-form;

    re2c:define:YYBACKUP     = "YYBACKUP";
    re2c:define:YYBACKUPCTX  = "YYBACKUPCTX";
    re2c:define:YYLESSTHAN   = "YYLESSTHAN";
    re2c:define:YYMTAGN      = "YYMTAGN";
    re2c:define:YYMTAGP      = "YYMTAGP";
    re2c:define:YYPEEK       = "*YYCURSOR";
    re2c:define:YYRESTORE    = "YYRESTORE";
    re2c:define:YYRESTORECTX = "YYRESTORECTX";
    re2c:define:YYRESTORETAG = "YYRESTORETAG";
    re2c:define:YYSKIP       = "++YYCURSOR;";
    re2c:define:YYSHIFT      = "YYSHIFT";
    re2c:define:YYCOPYMTAG   = "YYCOPYMTAG";
    re2c:define:YYCOPYSTAG   = "YYCOPYSTAG";
    re2c:define:YYSHIFTMTAG  = "YYSHIFTMTAG";
    re2c:define:YYSHIFTSTAG  = "YYSHIFTSTAG";
    re2c:define:YYSTAGN      = "YYSTAGN";
    re2c:define:YYSTAGP      = "YYSTAGP";

    any_white_space = [ \t\r\n];
    white_space = [ \t];
*/

template<char symbol>
constexpr auto lex_symbol_error = "expected symbol: "_sl + StringLiteral{symbol};

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
inline const char* lex_symbol (const char* YYCURSOR) {
    loop:
    switch (*YYCURSOR) {
        case 0:
            UNEXPECTED_INPUT("unexpected end of input");
        case '\t':
        case '\n':
        case '\r':
        case ' ':
            ++YYCURSOR;
            goto loop;
        case symbol:
            return ++YYCURSOR;
        default: {
            UNEXPECTED_INPUT(error_message.to_string_view());
        }
    }
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
inline const char* lex_same_line_symbol (const char* YYCURSOR) {
    loop:
    switch (*YYCURSOR) {
        case 0:
            UNEXPECTED_INPUT("unexpected end of input");
        case '\t':
        case ' ':
            ++YYCURSOR;
            goto loop;
        case symbol:
            return ++YYCURSOR;
        default: {
            UNEXPECTED_INPUT(error_message.sv());
        }
    }
}

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
inline const char* skip_white_space (const char* YYCURSOR) {
    /*!local:re2c
        white_space* { return YYCURSOR; }
    */
    std::unreachable();
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
inline const char* skip_any_white_space (const char* YYCURSOR) {
    /*!local:re2c
        any_white_space* { return YYCURSOR; }
    */
    std::unreachable();
}

inline const char* lex_white_space (const char* YYCURSOR) {
    /*!local:re2c
        white_space+ { return YYCURSOR; }
        * { UNEXPECTED_INPUT("expected white space"); }
    */
    std::unreachable();
}

inline const char* lex_any_white_space (const char* YYCURSOR) {
    /*!local:re2c
        any_white_space+ { return YYCURSOR; }
        * { UNEXPECTED_INPUT("expected any white space"); }
    */
    std::unreachable();
}

}
