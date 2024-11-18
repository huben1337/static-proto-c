#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include "string_helpers.cpp"

#define INTERNAL_ERROR(msg) printf(msg); exit(1);
#define UNEXPECTED_INPUT(msg) show_input_error(msg, YYCURSOR); exit(1);

extern const char *input_start;
extern std::string file_path_string;

void show_input_error (const char *msg, const char *error, char *error_end = 0) {
    if (!input_start) {
        INTERNAL_ERROR("input_start not set\n");
    }
    if (file_path_string.empty()) {
        INTERNAL_ERROR("file_name not set\n");
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
    printf("\n\033[97m%s:%d:%d\033[0m \033[91merror:\033[97m %s\033[0m\n  %s\n\033[%dC\033[31m^\033[0m", file_path_string.c_str(), line + 1, column + 1, msg, extract_string(start, end).c_str(), column + 2);
    exit(1);
}