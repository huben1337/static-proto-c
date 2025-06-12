#pragma once
#include <cstddef>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <array>
#include <string_view>
#include "string_literal.cpp"
#include "logger.cpp"

#define UNEXPECTED_INPUT(msg) show_syntax_error(msg, YYCURSOR);

static const char *input_start;// = nullptr;
constexpr auto input_file_path_default = "<unknown>"_sl;
static std::array<char, PATH_MAX> input_file_path = (input_file_path_default + string_lieral_of<'\0', PATH_MAX - input_file_path_default.size()>()).to_array();

[[noreturn]] static void show_syntax_error (const std::string_view& msg, const char *error, size_t error_squiggles = 0) {
    if (!input_start) {
        logger::error("[show_syntax_error] called before input_start set");
    }

    const char *start = error;
    const char *end = error;
    while (1) {
        if (start == input_start) break;
        if (*start == '\n') {
            if (start == error) {
                end++;
            }
            start++;
            break;
        }
        start--;
    }

    uint64_t line = 0;
    for (const char* cursor = input_start; cursor < start; cursor++) {
        if (*cursor == '\n') {
            line++;
        }
    }

    uint64_t column = error - start;

    
    for (; end > start; end++) {
        const char c = *end;
        if (c == '\n' || c == 0) break;
    }
    logger::info("diff ", end - start, " bytes");
    logger::log<true, true>("\n\033[97m", input_file_path.data(), ":", line + 1, ":", column + 1, "\033[0m \033[91merror:\033[97m ", msg, "\033[0m\n  ", std::string_view{start, end}, "\n\033[", column + 2,"C\033[31m^");
    for (size_t i = 0; i < error_squiggles; i++) {
        logger::log<true, true>("~");
    }
    logger::log<true, true>("\033[0m\n");
    logger::flush();
    exit(1);
}

[[noreturn]] static void show_syntax_error (const std::string_view& msg, const char *error, const char* error_squiggle_end) {
    if (error_squiggle_end > error) {
        show_syntax_error(msg, error, error_squiggle_end - error);
    } else {
        show_syntax_error(msg, error);
    }
}
