#pragma once

#include <cstddef>

#include <string_view>
#include "../global.hpp"
#include "../util/logger.hpp"

#if IS_MINGW

#include <cstdio>
#include <cstlib>

#else

#include <linux/limits.h>

#endif


#define UNEXPECTED_INPUT(msg) show_syntax_error(msg, YYCURSOR);


[[noreturn]] static void show_syntax_error (const std::string_view& msg, const char* const error, const size_t error_squiggles = 0) {
    BSSERT(global::input::start != nullptr, "[show_syntax_error] called before input_start set");

    const char *start = error;
    const char *end = error;
    for (; start != global::input::start; start--) {
        if (*start != '\n') continue;
        if (start == error) {
            end++;
        }
        start++;
        break;
    }

    uint64_t line = 0;
    for (const char* cursor = global::input::start; cursor < start; cursor++) {
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
    logger::log<true, true>("\n\033[97m", global::input::file_path, ":", line + 1, ":", column + 1, "\033[0m \033[91merror:\033[97m ", msg, "\033[0m\n  ", std::string_view{start, end}, "\n\033[", column + 2,"C\033[31m^");
    for (size_t i = 0; i < error_squiggles; i++) {
        logger::log<true, true>("~");
    }
    logger::log<true, true>("\033[0m\n");
    logger::flush();
    exit(1);
}

[[noreturn]] static void show_syntax_error (const std::string_view& msg, const char* const error, const char* const error_squiggle_end) {
    if (error_squiggle_end > error) {
        show_syntax_error(msg, error, error_squiggle_end - error);
    } else {
        show_syntax_error(msg, error);
    }
}
