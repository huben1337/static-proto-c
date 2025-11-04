#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include "../util/logger.hpp"

#if IS_MINGW

#include <cstdio>
#include <cstlib>

#else

#include <linux/limits.h>

#endif


#define UNEXPECTED_INPUT(msg) show_syntax_error(msg, YYCURSOR);

static const char* input_start = nullptr;
static std::string input_file_path {"<unknown>"};

[[noreturn]] static void show_syntax_error (const std::string_view& msg, const char *error, size_t error_squiggles = 0) {
    if (input_start == nullptr) {
        logger::error("[show_syntax_error] called before input_start set");
    }

    const char *start = error;
    const char *end = error;
    while (true) {
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
    logger::debug("diff ", end - start, " bytes");
    logger::log<true, true>("\n\033[97m", input_file_path, ":", line + 1, ":", column + 1, "\033[0m \033[91merror:\033[97m ", msg, "\033[0m\n  ", std::string_view{start, end}, "\n\033[", column + 2,"C\033[31m^");
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
