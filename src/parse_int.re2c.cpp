
#pragma once

#include <cstddef>
#include <concepts>
#include <limits>
#include "lex_result.cpp"
#include "lex_error.cpp"
#include "constexpr_helpers.cpp"

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
*/

#define RETURN_VALUE_CHECKED(BASE, DIGITS)                                                              \
if constexpr (max != std::numeric_limits<T>::max()) {                                                   \
    if (value > max) {                                                                                  \
        show_syntax_error("value out of range", YYCURSOR - 2 - DIGITS, DIGITS);                         \
    }                                                                                                   \
}                                                                                                       \
if constexpr (min != std::numeric_limits<T>::min()) {                                                   \
    if (value < min) {                                                                                  \
        show_syntax_error("value out of range", YYCURSOR - 2 - DIGITS, DIGITS);                         \
    }                                                                                                   \
}                                                                                                       \
return { YYCURSOR - 1, value };


template <std::unsigned_integral T, const T max, const T min>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_bin_digits_unsigned (char *YYCURSOR) {
    T value;
    /*!local:re2c
        [0]         { goto only_zeros;                                          }
        [1]         { value = 1; goto not_only_zeros;                           }
        *           { UNEXPECTED_INPUT("expected at least one binary digit");   }
    */
    only_zeros: {
        /*!local:re2c
            [0]         { goto only_zeros;                  }
            [1]         { value = 1; goto not_only_zeros;   }
            *           { return { YYCURSOR - 1, 0 };       }
        */
    }
    not_only_zeros:
    #pragma unroll
    for (size_t i = 1; i < uint_log2<max> + 2; i++) {
        /*!local:re2c
            [0]         { value = value << 1;       continue;   }
            [1]         { value = (value << 1) + 1; continue;   }
            *           { RETURN_VALUE_CHECKED(2, i)            }
        */
    }
    UNEXPECTED_INPUT("value overflow");
}

template <std::signed_integral T, const T max, const T min, size_t digits>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_trimmed_bin_digits_signed_negative_early_overflow (T value, char *YYCURSOR) {
    constexpr size_t max_digits = uint_log2<max> + 1;
    #pragma unroll
    for (size_t i = digits; i < max_digits; i++) {
        /*!local:re2c
            [0]         { value = value << 1;       continue;   }
            [1]         { value = (value << 1) - 1; continue;   }
            *           { RETURN_VALUE_CHECKED(2, i)            }
        */
    }
    /*!local:re2c
        [0]         { value = value << 1; goto expect_end;      }
        [1]         { UNEXPECTED_INPUT("value overflow");       }
        *           { RETURN_VALUE_CHECKED(2, max_digits - 1)   }
    */
    expect_end:
    /*!local:re2c
        [01]        { UNEXPECTED_INPUT("value overflow");   }
        *           { RETURN_VALUE_CHECKED(2, max_digits)   }
    */

}

template <std::signed_integral T, const T max, const T min, size_t digits>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_trimmed_bin_digits_signed_negative (T value, char *YYCURSOR) {
    if constexpr (digits < uint_log2<max> + 1) {
        /*!local:re2c
            [0]         { value = value << 1;       return lex_trimmed_bin_digits_signed_negative<T, max, min, digits + 1>(value, YYCURSOR);                }
            [1]         { value = (value << 1) - 1; return lex_trimmed_bin_digits_signed_negative_early_overflow<T, max, min, digits + 1>(value, YYCURSOR); }
            *           { RETURN_VALUE_CHECKED(2, digits)                                                                                                   }
        */
    } else {
        /*!local:re2c
            [0]         { value = value << 1; goto expect_end;  }
            [1]         { UNEXPECTED_INPUT("value overflow");   }
            *           { RETURN_VALUE_CHECKED(2, digits)       }
        */
        expect_end:
        /*!local:re2c
            [01]        { UNEXPECTED_INPUT("value overflow");   }
            *           { RETURN_VALUE_CHECKED(2, digits + 1)   }
        */
    }
}

template <std::signed_integral T, const T max, const T min>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_trimmed_bin_digits_signed_positive (char *YYCURSOR) {
    T value = 1;
    #pragma unroll
    for (size_t i = 1; i < uint_log2<max> + 2; i++) {
        /*!local:re2c
            [0]         { value = value << 1;       continue;   }
            [1]         { value = (value << 1) + 1; continue;   }
            *           { RETURN_VALUE_CHECKED(2, i)            }
        */
    }
    UNEXPECTED_INPUT("value overflow");
}

template <std::signed_integral T, const T max, const T min, bool is_negative>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_bin_digits_signed (char *YYCURSOR) {
    /*!local:re2c
        [0]         { goto only_zeros;                                                                                                                                                                              }
        [1]         { if constexpr (is_negative) { return lex_trimmed_bin_digits_signed_negative<T, max, min, 1>(-1, YYCURSOR); } else { return lex_trimmed_bin_digits_signed_positive<T, max, min>(YYCURSOR); }    }
        *           { UNEXPECTED_INPUT("expected at least one binary digit");                                                                                                                                       }
    */
    only_zeros: {
        /*!local:re2c
            [0]         { goto only_zeros;                                                                                                                                                                              }
            [1]         { if constexpr (is_negative) { return lex_trimmed_bin_digits_signed_negative<T, max, min, 1>(-1, YYCURSOR); } else { return lex_trimmed_bin_digits_signed_positive<T, max, min>(YYCURSOR); }    }
            *           { return { YYCURSOR - 1, 0 };                                                                                                                                                                   }
        */
    }
}


template <std::unsigned_integral T, const T max, const T min>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_oct_digits_unsigned (char *YYCURSOR) {
    T value;
    // Octal always has a leading zero. So no need to make sure the number has at least one digit.
    only_zeros:{
        /*!local:re2c
            [0]         { goto only_zeros;                              }
            [1-7]       { value = yych - '0';   goto not_only_zeros;    }
            *           { return { YYCURSOR - 1, 0 };                   }
        */
    }
    not_only_zeros:
    #pragma unroll
    for (size_t i = 1; i < uint_log<8, max> + 2; i++) {
        /*!local:re2c
            [0-7]       { value = value << 3 | yych - '0';  continue;   }
            *           { RETURN_VALUE_CHECKED(8, i)                    }
        */
    }
    UNEXPECTED_INPUT("value overflow");
}

template <std::signed_integral T, const T max, const T min, bool can_overflow_early>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_trimmed_oct_digits_signed_negative (T value, char *YYCURSOR) {
    constexpr size_t max_digits = uint_log<8, max> + (can_overflow_early ? 0 : 1);
    #pragma unroll
    for (size_t i = 0; i < uint_log<8, max> + (can_overflow_early ? 0 : 1); i++) {
        /*!local:re2c
            [0-7]       { value = (value << 3) - yych + '0';    continue;   }
            *           { RETURN_VALUE_CHECKED(8, i)                         }
        */
    }
    /*!local:re2c
        [0]         { value = value << 3; goto expect_end;      }
        [1-7]       { UNEXPECTED_INPUT("value overflow");       }
        *           { RETURN_VALUE_CHECKED(8, max_digits - 1)   }
    */
    expect_end:
    /*!local:re2c
        [0-7]       { UNEXPECTED_INPUT("value overflow");   }
        *           { RETURN_VALUE_CHECKED(8, max_digits)   }
    */

}

template <std::signed_integral T, const T max, const T min>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_trimmed_oct_digits_signed_positive (T value, char *YYCURSOR) {
    #pragma unroll
    for (size_t i = 1; i < uint_log<8, max> + 2; i++) {
        /*!local:re2c
            [0-7]       { value = (value << 3) | yych - '0';    continue;   }
            *           { RETURN_VALUE_CHECKED(8, i)                        }
        */
    }
    UNEXPECTED_INPUT("value overflow");
}

template <std::signed_integral T, const T max, const T min, bool is_negative>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_oct_digits_signed (char *YYCURSOR) {
    only_zeros: {
        /*!local:re2c
            [0]         { goto only_zeros;                                                                                                                                                                                                          }
            [1-3]       { if constexpr (is_negative) { return lex_trimmed_oct_digits_signed_negative<T, max, min, false>(-yych + '0', YYCURSOR); } else { return lex_trimmed_oct_digits_signed_positive<T, max, min>(yych - '0'     , YYCURSOR); }  }
            [3-7]       { if constexpr (is_negative) { return lex_trimmed_oct_digits_signed_negative<T, max, min, true >(-yych + '0', YYCURSOR); } else { return lex_trimmed_oct_digits_signed_positive<T, max, min>(yych - '0'     , YYCURSOR); }  }
            *           { return { YYCURSOR - 1, 0 };                                                                                                                                                                                               }
        */
    }
}

template <std::unsigned_integral T, const T max, const T min>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_hex_digits_unsigned (char *YYCURSOR) {
    T value;
    /*!local:re2c
        [0]         { goto only_zeros; }
        [1-9]       { value = yych - '0';      goto not_only_zeros;                 }
        [a-f]       { value = yych - 'a' + 10; goto not_only_zeros;                 }
        [A-F]       { value = yych - 'A' + 10; goto not_only_zeros;                 }
        *           { UNEXPECTED_INPUT("expected at least one hexadecimal digit");  }
    */
    only_zeros: {
        /*!local:re2c
            [0]         { goto only_zeros; }
            [1-9]       { value = yych - '0';      goto not_only_zeros; }
            [a-f]       { value = yych - 'a' + 10; goto not_only_zeros; }
            [A-F]       { value = yych - 'A' + 10; goto not_only_zeros; }
            *           { return { YYCURSOR - 1, 0 };                   }
        */
    }
    not_only_zeros:
    #pragma unroll
    for (size_t i = 1; i < uint_log<16, max> + 2; i++) {
        /*!local:re2c
            [0-9]       { value = (value << 4) + yych - '0';        continue;   }
            [a-f]       { value = (value << 4) + yych - 'a' + 10;   continue;   }
            [A-F]       { value = (value << 4) + yych - 'A' + 10;   continue;   }
            *           { RETURN_VALUE_CHECKED(16, i)                           }
        */
    }
    UNEXPECTED_INPUT("value overflow");
}

template <std::signed_integral T, const T max, const T min, bool can_overflow_early>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_trimmed_hex_digits_signed_negative (T value, char *YYCURSOR) {
    constexpr size_t max_digits = uint_log<16, max> + (can_overflow_early ? 0 : 1);
    #pragma unroll
    for (size_t i = 1; i < max_digits; i++) {
        /*!local:re2c
            [0-9]       { value = (value << 4) - yych + '0';      continue;   }
            [a-f]       { value = (value << 4) - yych + 'a' - 10; continue;   }
            [A-F]       { value = (value << 4) - yych + 'A' - 10; continue;   }
            *           { RETURN_VALUE_CHECKED(16, i)                         }
        */
    }
    /*!local:re2c
        [0]             { value = value << 4; goto expect_end;      }
        [1-9a-fA-F]     { UNEXPECTED_INPUT("value overflow");       }
        *               { RETURN_VALUE_CHECKED(16, max_digits - 1)  }
    */
    expect_end:
    /*!local:re2c
        [0-9a-fA-F]     { UNEXPECTED_INPUT("value overflow");   }
        *               { RETURN_VALUE_CHECKED(16, max_digits)  }
    */

}

template <std::signed_integral T, const T max, const T min>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_trimmed_hex_digits_signed_positive (T value, char *YYCURSOR) {
    #pragma unroll
    for (size_t i = 1; i < uint_log<16, max> + 2; i++) {
        /*!local:re2c
            [0-9]       { value = (value << 4) | yych - '0';        continue;   }
            [a-f]       { value = (value << 4) | yych - 'a' + 10;   continue;   }
            [A-F]       { value = (value << 4) | yych - 'A' + 10;   continue;   }
            *           { RETURN_VALUE_CHECKED(16, i)                           }
        */
    }
    UNEXPECTED_INPUT("value overflow");
}

template <std::signed_integral T, const T max, const T min, bool is_negative>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_hex_digits_signed (char *YYCURSOR) {
    /*!local:re2c
        [0]         { goto only_zeros;                                                                                                                                                                                                              }
        [1-7]       { if constexpr (is_negative) { return lex_trimmed_hex_digits_signed_negative<T, max, min, false>(-yych + '0'     , YYCURSOR); } else { return lex_trimmed_hex_digits_signed_positive<T, max, min>(yych - '0'     , YYCURSOR); } }
        [8-9]       { if constexpr (is_negative) { return lex_trimmed_hex_digits_signed_negative<T, max, min, true >(-yych + '0'     , YYCURSOR); } else { return lex_trimmed_hex_digits_signed_positive<T, max, min>(yych - '0'     , YYCURSOR); } }
        [a-f]       { if constexpr (is_negative) { return lex_trimmed_hex_digits_signed_negative<T, max, min, true >(-yych + 'a' - 10, YYCURSOR); } else { return lex_trimmed_hex_digits_signed_positive<T, max, min>(yych - 'a' + 10, YYCURSOR); } }
        [A-F]       { if constexpr (is_negative) { return lex_trimmed_hex_digits_signed_negative<T, max, min, true >(-yych + 'A' - 10, YYCURSOR); } else { return lex_trimmed_hex_digits_signed_positive<T, max, min>(yych - 'A' + 10, YYCURSOR); } }
        *           { UNEXPECTED_INPUT("expected at least one hexadecimal digit");                                                                                                                                                                  }
    */
    only_zeros: {
        /*!local:re2c
            [0]         { goto only_zeros;                                                                                                                                                                                                              }
            [1-7]       { if constexpr (is_negative) { return lex_trimmed_hex_digits_signed_negative<T, max, min, false>(yych - '0'      , YYCURSOR); } else { return lex_trimmed_hex_digits_signed_positive<T, max, min>(yych - '0'     , YYCURSOR); } }
            [8-9]       { if constexpr (is_negative) { return lex_trimmed_hex_digits_signed_negative<T, max, min, true >(-yych + '0'     , YYCURSOR); } else { return lex_trimmed_hex_digits_signed_positive<T, max, min>(yych - '0'     , YYCURSOR); } }
            [a-f]       { if constexpr (is_negative) { return lex_trimmed_hex_digits_signed_negative<T, max, min, true >(-yych + 'a' - 10, YYCURSOR); } else { return lex_trimmed_hex_digits_signed_positive<T, max, min>(yych - 'a' + 10, YYCURSOR); } }
            [A-F]       { if constexpr (is_negative) { return lex_trimmed_hex_digits_signed_negative<T, max, min, true >(-yych + 'A' - 10, YYCURSOR); } else { return lex_trimmed_hex_digits_signed_positive<T, max, min>(yych - 'A' + 10, YYCURSOR); } }
            *           { return { YYCURSOR - 1, 0 };                                                                                                                                                                                                   }
        */
    }
}

template <std::unsigned_integral T, const T max, const T min>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_dec_digits_unsigned (T value, char *YYCURSOR) {
    constexpr size_t max_digits = uint_log10<max>;
    #pragma unroll
    for (size_t i = 1; i < max_digits; i++) {
        /*!local:re2c
            [0-9]       { value = value * 10 + yych - '0';  continue;   }
            *           { RETURN_VALUE_CHECKED(10, i)                   }
        */
    }
    /*!local:re2c
        [0-9]       { T new_value = value * 10 + yych - '0';    if (new_value < value) { UNEXPECTED_INPUT("value overflow"); }     value = new_value; goto expect_end;  }
        *           { RETURN_VALUE_CHECKED(10, max_digits - 1)                                                                                                          }
    */
    expect_end:
    /*!local:re2c
        [0-9]       { UNEXPECTED_INPUT("value overflow");           }
        *           { RETURN_VALUE_CHECKED(10, max_digits)          }
    */
}

template <std::signed_integral T, const T max, const T min>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_dec_digits_signed_positive (T value, char *YYCURSOR) {
    constexpr size_t max_digits = uint_log10<max>;
    #pragma unroll
    for (size_t i = 1; i < max_digits; i++) {
        /*!local:re2c
            [0-9]       { value = value * 10 + yych - '0';  continue;   }
            *           { RETURN_VALUE_CHECKED(10, i)                   }
        */
    }
    /*!local:re2c
        [0-9]       { T new_value = value * 10 + yych - '0';  if (new_value < value) { UNEXPECTED_INPUT("value overflow"); } else { value = new_value; goto expect_end; }   }
        *           { RETURN_VALUE_CHECKED(10, max_digits - 1)                                                                                                              }
    */
    expect_end:
    /*!local:re2c
        [0-9]       { UNEXPECTED_INPUT("value overflow");   }
        *           { RETURN_VALUE_CHECKED(10, max_digits)  }
    */
}

template <std::signed_integral T, const T max, const T min>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_dec_digits_signed_negative (T value, char *YYCURSOR) {
    constexpr size_t max_digits = uint_log10<max>;
    #pragma unroll
    for (size_t i = 1; i < max_digits; i++) {
        /*!local:re2c
            [0-9]       { value = value * 10 - yych + '0';  continue;   }
            *           { RETURN_VALUE_CHECKED(10, i)                   }
        */
    }
    /*!local:re2c
        [0-9]       { T new_value = value * 10 - yych + '0';  if (new_value > value) { UNEXPECTED_INPUT("value overflow"); } else { value = new_value; goto expect_end; }   }
        *           { RETURN_VALUE_CHECKED(10, max_digits - 1)                                                                                                              }
    */
    expect_end:
    /*!local:re2c
        [0-9]       { UNEXPECTED_INPUT("value overflow");   }
        *           { RETURN_VALUE_CHECKED(10, max_digits)  }
    */
}


template <std::unsigned_integral T, const T max, const T min>
[[clang::always_inline]] [[gnu::always_inline]] INLINE LexResult<T> lex_dec_digits_unsigned_ (T value, char *YYCURSOR) {
    #pragma unroll
    for (size_t i = 1; i < uint_log10<max> + 2; i++) {
        /*!local:re2c
            [0-9]       { T new_value = value * 10 + yych - '0';    if (new_value < value) { UNEXPECTED_INPUT("value overflow"); }     value = new_value;  continue;    }
            *           { RETURN_VALUE_CHECKED(10, i)                                                                                                                   }
        */
    }
    UNEXPECTED_INPUT("value overflow");
}

template <std::unsigned_integral T, const T max = std::numeric_limits<T>::max(), const T min = std::numeric_limits<T>::min()>
INLINE LexResult<T> parse_uint (char *YYCURSOR) {
    /*!local:re2c
        "0b"        { return lex_bin_digits_unsigned<T, max, min>(YYCURSOR);                }
        "0"         { return lex_oct_digits_unsigned<T, max, min>(YYCURSOR);                }
        [1-9]       { return lex_dec_digits_unsigned<T, max, min>(yych - '0', YYCURSOR);    }
        "0x"        { return lex_hex_digits_unsigned<T, max, min>(YYCURSOR);                }
        *           { UNEXPECTED_INPUT("expected unsigned integer literal");                }
    */
}

template <std::signed_integral T, const T max = std::numeric_limits<T>::max(), const T min = std::numeric_limits<T>::min()>
INLINE LexResult<T> parse_int (char *YYCURSOR) {
    /*!local:re2c
        "-"         { goto minus_sign; }
        "0b"        { return lex_bin_digits_signed<T, max, min, false>(YYCURSOR);                   }
        "0"         { return lex_oct_digits_signed<T, max, min, false>(YYCURSOR);                   }
        [1-9]       { return lex_dec_digits_signed_positive<T, max, min>(yych - '0', YYCURSOR);     }
        "0x"        { return lex_hex_digits_signed<T, max, min, false>(YYCURSOR);                   }
        *           { UNEXPECTED_INPUT("expected signed integer literal");                          }
    */
    minus_sign: {
        /*!local:re2c
            "0b"        { return lex_bin_digits_signed<T, max, min, true>(YYCURSOR);                    }
            "0"         { return lex_oct_digits_signed<T, max, min, true>(YYCURSOR);                    }
            [1-9]       { return lex_dec_digits_signed_negative<T, max, min>('0' - yych, YYCURSOR);     }
            "0x"        { return lex_hex_digits_signed<T, max, min, true>(YYCURSOR);                    }
            *           { UNEXPECTED_INPUT("expected signed integer literal");                          }
        */
    }
}
