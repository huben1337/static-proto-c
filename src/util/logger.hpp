#pragma once

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gsl/util>
#include <string_view>
#include <utility>
#include <type_traits>
#include <limits>
#include <concepts>
#include <immintrin.h>
#include <boost/preprocessor/stringize.hpp>

#include "../base.hpp"
#include "../util/string_literal.hpp"
#include "../fast_math/log.hpp"

#if IS_MINGW

#include "../windows/io.hpp"
#include <windows.h>
#include <minwindef.h>
#include <synchapi.h>
#include <winnt.h>
#include <io.h>

#else

#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

#endif

namespace escape_sequences {
    namespace colors {
        namespace basic {
            namespace foreground {
                constexpr auto black    = "30"_sl;
                constexpr auto red      = "31"_sl;
                constexpr auto green    = "32"_sl;
                constexpr auto yellow   = "33"_sl;
                constexpr auto blue     = "34"_sl;
                constexpr auto magenta  = "35"_sl;
                constexpr auto cyan     = "36"_sl;
                constexpr auto white    = "37"_sl;
                constexpr auto _default = "39"_sl;
            }
            namespace background {
                constexpr auto black    = "40"_sl;
                constexpr auto red      = "41"_sl;
                constexpr auto green    = "42"_sl;
                constexpr auto yellow   = "43"_sl;
                constexpr auto blue     = "44"_sl;
                constexpr auto magenta  = "45"_sl;
                constexpr auto cyan     = "46"_sl;
                constexpr auto white    = "47"_sl;
                constexpr auto _default = "49"_sl;
            }
        }

        namespace bright {
            namespace foreground {
                constexpr auto black   = "90"_sl;
                constexpr auto red     = "91"_sl;
                constexpr auto green   = "92"_sl;
                constexpr auto yellow  = "93"_sl;
                constexpr auto blue    = "94"_sl;
                constexpr auto magenta = "95"_sl;
                constexpr auto cyan    = "96"_sl;
                constexpr auto white   = "97"_sl;
            }
            namespace background {
                constexpr auto black   = "100"_sl;
                constexpr auto red     = "101"_sl;
                constexpr auto green   = "102"_sl;
                constexpr auto yellow  = "103"_sl;
                constexpr auto blue    = "104"_sl;
                constexpr auto magenta = "105"_sl;
                constexpr auto cyan    = "106"_sl;
                constexpr auto white   = "107"_sl;
            }
        }

        namespace _256 {
            namespace {
                template<StringLiteral code>
                constexpr auto fgc = "38;5;"_sl + code;

                template<StringLiteral code>
                constexpr auto bgc = "48;5;"_sl + code;

                namespace _basic {
                    constexpr auto black   = "0"_sl;
                    constexpr auto red     = "1"_sl;
                    constexpr auto green   = "2"_sl;
                    constexpr auto yellow  = "3"_sl;
                    constexpr auto blue    = "4"_sl;
                    constexpr auto magenta = "5"_sl;
                    constexpr auto cyan    = "6"_sl;
                    constexpr auto white   = "7"_sl;
                }

                namespace _bright {
                    constexpr auto black   = "8"_sl;
                    constexpr auto red     = "9"_sl;
                    constexpr auto green   = "10"_sl;
                    constexpr auto yellow  = "11"_sl;
                    constexpr auto blue    = "12"_sl;
                    constexpr auto magenta = "13"_sl;
                    constexpr auto cyan    = "14"_sl;
                    constexpr auto white   = "15"_sl;
                }

                template <uint8_t v>
                constexpr uint8_t closest_cube6 = v / ( 256.0 / ( 216.0 / 43.0 ) );

                template <uint8_t r, uint8_t g, uint8_t b>
                constexpr auto _cube6 = 
                "2;"_sl + string_literal::from<closest_cube6<r>>
                + ";"_sl + string_literal::from<closest_cube6<g>>()
                + ";"_sl + string_literal::from<closest_cube6<b>>();

                template <uint8_t v>
                requires (v < 24)
                constexpr auto _gray_scale = "2;"_sl + string_literal::from<232 + v>;
            }

            namespace foreground {
                namespace basic {
                    constexpr auto black   = fgc<_basic::black>;
                    constexpr auto red     = fgc<_basic::red>;
                    constexpr auto green   = fgc<_basic::green>;
                    constexpr auto yellow  = fgc<_basic::yellow>;
                    constexpr auto blue    = fgc<_basic::blue>;
                    constexpr auto magenta = fgc<_basic::magenta>;
                    constexpr auto cyan    = fgc<_basic::cyan>;
                    constexpr auto white   = fgc<_basic::white>;
                }

                namespace bright {
                    constexpr auto black   = fgc<_bright::black>;
                    constexpr auto red     = fgc<_bright::red>;
                    constexpr auto green   = fgc<_bright::green>;
                    constexpr auto yellow  = fgc<_bright::yellow>;
                    constexpr auto blue    = fgc<_bright::blue>;
                    constexpr auto magenta = fgc<_bright::magenta>;
                    constexpr auto cyan    = fgc<_bright::cyan>;
                    constexpr auto white   = fgc<_bright::white>;
                }

                template <uint8_t r, uint8_t g, uint8_t b>
                constexpr auto cube6 = fgc<_cube6<r, g, b>>;

                template <uint8_t v>
                constexpr auto gray_scale = fgc<_gray_scale<v>>;
            }

            namespace background {
                namespace basic {
                    constexpr auto black   = bgc<_basic::black>;
                    constexpr auto red     = bgc<_basic::red>;
                    constexpr auto green   = bgc<_basic::green>;
                    constexpr auto yellow  = bgc<_basic::yellow>;
                    constexpr auto blue    = bgc<_basic::blue>;
                    constexpr auto magenta = bgc<_basic::magenta>;
                    constexpr auto cyan    = bgc<_basic::cyan>;
                    constexpr auto white   = bgc<_basic::white>;
                }

                namespace bright {
                    constexpr auto black   = bgc<_bright::black>;
                    constexpr auto red     = bgc<_bright::red>;
                    constexpr auto green   = bgc<_bright::green>;
                    constexpr auto yellow  = bgc<_bright::yellow>;
                    constexpr auto blue    = bgc<_bright::blue>;
                    constexpr auto magenta = bgc<_bright::magenta>;
                    constexpr auto cyan    = bgc<_bright::cyan>;
                    constexpr auto white   = bgc<_bright::white>;
                }

                template <uint8_t r, uint8_t g, uint8_t b>
                constexpr auto cube6 = bgc<_cube6<r, g, b>>;

                template <uint8_t v>
                constexpr auto gray_scale = bgc<_gray_scale<v>>;
            }
        }
    }
}

namespace logger_detail {
    constexpr size_t buffer_size = 1 << 12;
    constexpr size_t buffer_alignment =  std::max(buffer_size, 4096UL);

    namespace {
        alignas(buffer_alignment) char _buffer_a[buffer_size];
        alignas(buffer_alignment) char _buffer_b[buffer_size];

        char* buffer = _buffer_a;
        char* other_buffer = _buffer_b;

        char* buffer_end = buffer + buffer_size;
        char* buffer_dst = buffer;

        #ifdef _WINDOWS_

        const windows::io::FileHandle<
            windows::io::CreateFileParamsWithName<
                "CONOUT$",
                GENERIC_WRITE,
                FILE_SHARE_WRITE,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING
            >
        > stdout_handle {nullptr, nullptr};

        windows::io::AsyncFileWriter stdout_writer {stdout_handle};

        #else

        const int stdout_fd = []()->int {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
            int fd = ::open("/dev/stdout", O_WRONLY | O_NONBLOCK);
            if (fd == -1) {
                std::perror("[logger] error when trying to intialize stdout_fd. open of /dev/stdout failed");
                std::exit(1);
            }
            return fd;
        }();

        struct ::pollfd stdout_poll_desc = {
            .fd = stdout_fd,
            .events = POLLOUT,
            .revents = 0
        };

        #endif
    }


    class logger {
        public:

        logger () = delete;
        ~logger () = delete;

        template <StringLiteral name, StringLiteral color_code>
        static constexpr auto log_level = "\033["_sl + color_code + "m["_sl + name + "]\033[0m "_sl;

        static constexpr auto debug_prefix = log_level<"DEBUG", escape_sequences::colors::bright::foreground::yellow>;
        static constexpr auto info_prefix = log_level<"INFO", escape_sequences::colors::bright::foreground::green>;
        static constexpr auto error_prefix = log_level<"ERROR", escape_sequences::colors::bright::foreground::red>;
        static constexpr auto warn_prefix = log_level<"WARN", escape_sequences::colors::bright::foreground::magenta>;

        private:

        static void _handled_write_stdout (const char* src, size_t size) {
            #ifdef _WINDOWS_

            stdout_writer.write_handled(src, size);

            #else

            size_t left = size;
            try_write:
            auto write_result = ::write(stdout_fd, src, left);
            if (write_result >= 0) {
                if (gsl::narrow_cast<size_t>(write_result) == left) return;
                left -= write_result;
                src += write_result;
                goto try_write;
            }
            if (write_result == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    auto poll_result = ::poll(&stdout_poll_desc, 1, 10000);
                    if (poll_result == 1) [[likely]] goto try_write;
                    if (poll_result == 0) {
                        std::puts("[logger::_handled_write_stdout] poll timed out.");
                    } else if (poll_result == -1) {
                        std::perror("[logger::_handled_write_stdout] poll failed.");
                    } else {
                        std::puts("[logger::_handled_write_stdout] poll unexpected result.");
                    }
                } else {
                    std::perror("[logger::_handled_write_stdout] write failed.");
                }
            } else {
                std::puts("[logger::_handled_write_stdout] write unexpected result.");
            }
            std::exit(1);

            #endif
        }

        static void handled_write_buffer_stdout (size_t size) {
            _handled_write_stdout(buffer, size);
            std::swap(buffer, other_buffer);
            buffer_end = buffer + buffer_size;
            // buffer_dst has to be manualy set after calling this function - enables some micro optimizations
        }

        template<bool is_first, bool is_last>
        requires (is_first)
        static void write_string_view (const char* begin, size_t length) {
            if constexpr (is_last) {
                _handled_write_stdout(begin, length);
            } else {
                if (length >= buffer_size) {
                    _handled_write_stdout(begin, length);
                } else {
                    std::memcpy(buffer, begin, length);
                    buffer_dst += length;
                }
            }
        }

        template<bool is_first, bool is_last>
        requires (!is_first) 
        static void write_string_view (const char* begin, size_t length) {
            if (buffer_dst == buffer) {
                return write_string_view<true, is_last>(begin, length);
            } else {
                size_t free_space = buffer_end - buffer_dst;
                if (free_space >= length) {
                    if constexpr (is_last) {
                        std::memcpy(buffer_dst, begin, length);
                        handled_write_buffer_stdout(buffer_dst - buffer + length);
                        buffer_dst = buffer;
                    } else /* if constexpr (!is_last) */ {
                        std::memcpy(buffer_dst, begin, length);
                        buffer_dst += length;
                    }
                } else {
                    if constexpr (is_last) {
                        handled_write_buffer_stdout(buffer_dst - buffer);
                        buffer_dst = buffer;
                        _handled_write_stdout(begin, length);
                    } else {
                        std::memcpy(buffer_dst, begin, free_space);
                        handled_write_buffer_stdout(buffer_size);
                        size_t remaining = length - free_space;
                        if (remaining >= buffer_size) {
                            _handled_write_stdout(begin + free_space, remaining);
                            buffer_dst = buffer;
                        } else {
                            std::memcpy(buffer, begin + free_space, remaining);
                            buffer_dst = buffer + remaining;
                        }
                    }
                }
            }
        }

        template<bool is_first, bool is_last>
        [[gnu::always_inline]] static void write (const std::string_view& msg) {
            return write_string_view<is_first, is_last>(msg.data(), msg.size());
        }
        template<bool is_first, bool is_last>
        [[gnu::always_inline]] static void write (std::string_view&& msg) {
            return write_string_view<is_first, is_last>(msg.data(), msg.size());
        }
        template <bool is_first, bool is_last, size_t N>
        [[gnu::always_inline]] static void write (const char (&value)[N]) {
            return write_string_view<is_first, is_last>(static_cast<const char*>(value), N - 1);
        }
        template <bool is_first, bool is_last, size_t N>
        [[gnu::always_inline]] static void write (char (&&value)[N]) {
            return write_string_view<is_first, is_last>(static_cast<const char*>(value), N - 1);
        }
        template <bool is_first, bool is_last, size_t N>
        [[gnu::always_inline]] static void write (const StringLiteral<N>& value) {
            return write_string_view<is_first, is_last>(static_cast<const char*>(value.data), N - 1);
        }
        template <bool is_first, bool is_last, size_t N>
        [[gnu::always_inline]] static void write (StringLiteral<N>&& value) {
            return write_string_view<is_first, is_last>(static_cast<const char*>(value.data), N - 1);
        }

        template<bool is_first, bool is_last, char C>
        [[gnu::always_inline]] static void write_char () {
            if constexpr (is_last) {
                if constexpr (is_first) {
                    constexpr char char_buffer[1] = {C};
                    _handled_write_stdout(char_buffer, 1);
                } else {
                    *(buffer_dst++) = C;
                    handled_write_buffer_stdout(buffer_dst - buffer);
                    buffer_dst = buffer;
                }
            } else /* if constexpr (!is_last) */ {
                *(buffer_dst++) = C;
                if constexpr (!is_first || buffer_size <= 1) {
                    if (buffer_dst == buffer_end) {
                        handled_write_buffer_stdout(buffer_size);
                        buffer_dst = buffer;
                    }
                }
            }
        }

        template<bool is_first, bool is_last, bool is_negative, std::unsigned_integral T>
        static void write_nonzero (T value) {
            uint8_t log10_of_value = fast_math::log_unsafe<10>(value);
            constexpr size_t sign_size = is_negative ? 1 : 0;
            if constexpr (is_negative) {
                *(buffer_dst++) = '-';
            }
            size_t length = log10_of_value + 1;
            char* end; // End is only initialized if !is_first
            if constexpr (!is_first || buffer_size <= (ce::log10<std::numeric_limits<uint64_t>::max()> + sign_size)) {
                end = buffer_dst + length;
                if (end >= buffer_end) {
                    handled_write_buffer_stdout(buffer_dst - buffer);
                    buffer_dst = buffer;
                    end = buffer_dst + length;
                }
            }
            buffer_dst += log10_of_value;
            *(buffer_dst--) = '0' + (value % 10);
            value /= 10;
            while (value > 0) {
                *(buffer_dst--) = '0' + (value % 10);
                value /= 10;
            }
            if constexpr (is_last) {
                if constexpr (is_first) {
                    handled_write_buffer_stdout(length);
                } else /* if constexpr (!is_first) */ {
                    handled_write_buffer_stdout(end - buffer);
                }
                buffer_dst = buffer;
            } else {
                if constexpr (is_first) {
                    buffer_dst = buffer + length;
                } else {
                    buffer_dst = end;
                }
            }
        }

        template<bool is_first, bool is_last, std::unsigned_integral T>
        [[gnu::always_inline]] static void write (T value) {
            if (value == 0) {
                write_char<is_first, is_last, '0'>();
            } else {
                write_nonzero<is_first, is_last, false>(value);
            }
        }

        template<bool is_first, bool is_last, std::signed_integral T>
        [[gnu::always_inline]] static void write (T value) {
            using U = std::make_unsigned_t<T>;
            if (value == 0) {
                write_char<is_first, is_last, '0'>();
            } else if (value < 0) {
                write_nonzero<false, is_last, true>(U{0} - static_cast<U>(value));
            } else {
                write_nonzero<false, is_last, false>(static_cast<U>(value));
            }
        }

        template <bool is_first, bool is_last>
        [[gnu::always_inline]] static void write (bool value) {
            if (value) {
                write<is_first, is_last>("true"_sl);
            } else {
                write<is_first, is_last>("false"_sl);
            }
        }

        template <StringLiteral value, bool has_buffered, bool buffered, bool no_newline>
        [[gnu::always_inline]] static void _write_value () {
            if constexpr (no_newline) {
                write<!has_buffered, !buffered>(value + "\n"_sl);
            } else {
                write<!has_buffered, !buffered>(value);
            }
        }

        template <StringLiteral value, bool buffered, bool no_newline = false>
        [[gnu::always_inline]] static void write_values () {
            if (buffer_dst == buffer) {
                _write_value<value, false, buffered, no_newline>();
            } else {
                _write_value<value, true, buffered, no_newline>();
            }
        }

        template <StringLiteral prefix, bool has_buffered, bool buffered, bool no_newline, typename... T>
        requires (sizeof...(T) > 0 && !no_newline)
        [[gnu::always_inline]] static void _write_values (T&&... values) {
            write<!has_buffered, false>(std::move(prefix));
            (write<false, false>(std::forward<T>(values)), ...);
            write_char<false, !buffered, '\n'>();
        }

        template <bool buffered, typename FirstT, typename ...RestT>
        [[gnu::always_inline]] static void _write_rest_values (FirstT&& first, RestT&&... rest) {
            if constexpr (sizeof...(RestT) > 0) {
                write<false, false>(std::forward<FirstT>(first));
                _write_rest_values<buffered>(std::forward<RestT>(rest)...);
            } else {
                write<false, !buffered>(std::forward<FirstT>(first));
            }
        }

        template <StringLiteral prefix, bool has_buffered, bool buffered, bool no_newline, typename... T>
        requires (sizeof...(T) > 0 && no_newline)
        [[gnu::always_inline]] static void _write_values (T&&... values) {
            write<!has_buffered, false>(std::move(prefix));
            _write_rest_values<buffered>(std::forward<T>(values)...);
        }

        template <StringLiteral prefix, bool buffered, bool no_newline = false, typename... T>
        requires (sizeof...(T) > 0)
        [[gnu::always_inline]] static void write_values (T&&... values) {
            if (buffer_dst == buffer) {
                _write_values<prefix, false, buffered, no_newline>(std::forward<T>(values)...);
            } else {
                _write_values<prefix, true, buffered, no_newline>(std::forward<T>(values)...);
            }
        }

        public:
        template <bool no_newline = false, bool buffered = false, StringLiteral first_value, typename... T>
        static void log (T&&... values) {
            write_values<first_value, buffered, no_newline>(std::forward<T>(values)...);
        }
        template <bool no_newline = false, bool buffered = false,  typename... T>
        static void log (T&&... values) {
            write_values<"", buffered, no_newline>(std::forward<T>(values)...);
        }

        template <bool buffered = false, StringLiteral first_value, typename... T>
        static void info (T&&... values) {
            write_values<info_prefix + first_value, buffered>(std::forward<T>(values)...);
        }
        template <bool buffered = false, typename... T>
        static void info (T&&... values) {
            write_values<info_prefix, buffered>(std::forward<T>(values)...);
        }

        template <bool buffered = false, StringLiteral first_value, typename... T>
        static void debug (T&&... values) {
            write_values<debug_prefix + first_value, buffered>(std::forward<T>(values)...);
        }
        template <bool buffered = false, typename... T>
        static void debug (T&&... values) {
            write_values<debug_prefix, buffered>(std::forward<T>(values)...);
        }

        template <bool buffered = false, StringLiteral first_value, typename... T>
        static void warn (T&&... values) {
            write_values<warn_prefix + first_value, buffered>(std::forward<T>(values)...);
        }
        template <bool buffered = false, typename... T>
        static void warn (T&&... values) {
            write_values<warn_prefix, buffered>(std::forward<T>(values)...);
        }

        template <bool buffered = false, StringLiteral first_value, typename... T>
        static void error (T&&... values) {
            write_values<error_prefix + first_value, buffered>(std::forward<T>(values)...);
        }
        template <bool buffered = false, typename... T>
        static void error (T&&... values) {
            write_values<error_prefix, buffered>(std::forward<T>(values)...);
        }

        static void flush () {
            if (buffer_dst == buffer) return;
            handled_write_buffer_stdout(buffer_dst - buffer);
            buffer_dst = buffer;
        }
    };
};

using logger = logger_detail::logger;

template<StringLiteral auto_msg, typename... ArgsT>
[[noreturn, gnu::noinline, gnu::cold]] void bssert_fail (ArgsT&&... args) {
    logger::error<false, auto_msg + " with "_sl>(std::forward<ArgsT>(args)...);
    std::exit(1);
}

template<StringLiteral auto_msg>
[[noreturn, gnu::noinline, gnu::cold]] void bssert_fail () {
    logger::error<false, auto_msg>();
    std::exit(1);
}

#define BSSERT(EXPR, MORE...)                                                                       \
/* NOLINTNEXTLINE(readability-simplify-boolean-expr) */                                             \
if (!(EXPR)) {                                                                                      \
    bssert_fail<"Assertion " #EXPR " failed at " __FILE__ ":" BOOST_PP_STRINGIZE(__LINE__)>(MORE);  \
}
