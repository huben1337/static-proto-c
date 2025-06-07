#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <array>
#include "string_literal.cpp"
#include "logger.cpp"

#define UNEXPECTED_INPUT(msg) show_syntax_error(msg, YYCURSOR);

static const char *input_start;// = nullptr;
constexpr auto input_file_path_default = "<unknown>"_sl;
static std::array<char, PATH_MAX> input_file_path = (input_file_path_default + string_lieral_of<'\0', PATH_MAX - input_file_path_default.size()>()).to_array();

[[noreturn]] static void show_syntax_error (const char *msg, const char *error, char *error_end = 0) {
    if (!input_start) {
        logger::error("[show_syntax_error] called before input_start set");
    }

    const char *start = error;
    while (1) {
        if (start == input_start) break;
        if (*start == '\n') {
            start++;
            break;
        }
        start--;
    }

    uint64_t line = 0;
    for (auto i = input_start; i < start; i++) {
        if (*i == '\n') {
            line++;
        }
    }

    uint64_t column = error - start;

    const char *end = error;
    while (1) {
        if (*end == 0) goto print;
        if (*end == '\n') break; 
        end++;
    }
    print:
    printf("\n\033[97m%s:%llu:%llu\033[0m \033[91merror:\033[97m %s\033[0m\n  %s\n\033[%lluC\033[31m^\033[0m", input_file_path.data(), line + 1, column + 1, msg, std::string(start, end).c_str(), column + 2);
    exit(1);
}