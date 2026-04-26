#pragma once

#include <cerrno>
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
#include <boost/preprocessor/stringize.hpp>
#include <nameof.hpp>

#include "../util/string_literal.hpp"
#include "../fast_math/log.hpp"
#include "../estd/class_constraints.hpp"
#include "../estd/type_traits.hpp"
#include "./escape_sequences.hpp"

#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

template <typename T>
concept trivially_loggable =
       std::is_same_v<std::string_view, T>
    || std::is_constructible_v<std::string_view, T>
    || std::is_integral_v<T>
    || estd::is_char_array_v<T>
    || is_string_literal_v<T>;


class logger : estd::unique_only {
public:

    /* Example implementation of loggable
    ```template <typename writer_params>
        void log (const logger::writer<writer_params>& lw) const```
    */
    template <typename ParamsT>
    class writer {
    public:
        friend class ::logger;

    private:
        static constexpr bool outer_is_first = ParamsT::outer_is_first;
        static constexpr bool outer_is_last = ParamsT::outer_is_last;
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
        logger& target;

        [[gnu::always_inline]] constexpr explicit writer (logger& target) : target(target) {}

    public:
        template <bool is_first, bool is_last, typename FirstT, typename... RestT>
        [[gnu::always_inline]] void write (FirstT&& first, RestT&&... rest) const {
            constexpr bool has_rest = sizeof...(RestT) > 0;
            target.write<
                outer_is_first && is_first,
                outer_is_last && is_last && !has_rest
            >(std::forward<FirstT>(first));
            if constexpr (has_rest) {
                write<false, is_last>(std::forward<RestT>(rest)...);
            }
        }
    };

    template <bool outer_is_first_, bool outer_is_last_>
    struct writer_params {
        static constexpr bool outer_is_first = outer_is_first_;
        static constexpr bool outer_is_last = outer_is_last_;
    };

    static constexpr size_t buffer_size = 1 << 12;
    static constexpr size_t buffer_alignment = std::max(buffer_size, 4096UL);

    template <StringLiteral name, StringLiteral style>
    static constexpr auto log_level = string_literal::concat_v<style, "["_sl, name, "]"_sl, escape_sequences::mode::reset, " "_sl>;

    static constexpr auto debug_prefix = log_level<"DEBUG", escape_sequences::style<escape_sequences::colors::foreground::yellow ::bright>::value>;
    static constexpr auto info_prefix  = log_level<"INFO" , escape_sequences::style<escape_sequences::colors::foreground::green  ::bright>::value>;
    static constexpr auto error_prefix = log_level<"ERROR", escape_sequences::style<escape_sequences::colors::foreground::red    ::bright>::value>;
    static constexpr auto warn_prefix  = log_level<"WARN" , escape_sequences::style<escape_sequences::colors::foreground::magenta::bright>::value>;

private:
    alignas(64) char _buffer_a[buffer_size] {};
    alignas(64) char _buffer_b[buffer_size] {};

    char* buffer = _buffer_a;
    char* other_buffer = _buffer_b;

    char* buffer_end = buffer + buffer_size;
    char* buffer_dst = buffer;

    struct ::pollfd output_pollfd;

    static int open_output_file (const char* const output_path) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        int fd = ::open(output_path, O_WRONLY | O_NONBLOCK);
        if (fd == -1) {
            std::perror("[logger::open_output_file] error when trying to intialize fd.");
            std::exit(1);
        }
        return fd;
    }

public:
    explicit logger (const int output_fd)
    : output_pollfd({
        .fd = output_fd,
        .events = POLLOUT,
        .revents = 0
    }) {}

    explicit logger (const char* const output_path)
        : logger{open_output_file(output_path)} {}

private:
    void _handled_write_stdout (const char* src, size_t left) {
        try_write:
        const ssize_t write_result = ::write(output_pollfd.fd, src, left);
        if (write_result >= 0) {
            const size_t written = gsl::narrow_cast<size_t>(write_result);
            if (written == left) return;
            left -= written;
            src += written;
            goto try_write;
        }
        if (write_result == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                auto poll_result = ::poll(&output_pollfd, 1, 10000);
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
    }

    void handled_write_buffer_stdout (size_t size) {
        _handled_write_stdout(buffer, size);
        std::swap(buffer, other_buffer);
        buffer_end = buffer + buffer_size;
        // buffer_dst has to be manualy set after calling this function - enables some micro optimizations
    }

    template <bool is_first, bool is_last>
    requires (is_first)
    void write_string_view (const char* begin, size_t length) {
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

    [[nodiscard]] constexpr size_t buffered_size () const {
        return gsl::narrow_cast<size_t>(buffer_dst - buffer);
    }

    template <bool is_first, bool is_last>
    requires (!is_first)
    void write_string_view (const char* begin, size_t length) {
        if (buffer_dst == buffer) {
            return write_string_view<true, is_last>(begin, length);
        }
        size_t free_space = gsl::narrow_cast<size_t>(buffer_end - buffer_dst);
        if (free_space >= length) {
            if constexpr (is_last) {
                std::memcpy(buffer_dst, begin, length);
                handled_write_buffer_stdout(buffered_size() + length);
                buffer_dst = buffer;
            } else /* if constexpr (!is_last) */ {
                std::memcpy(buffer_dst, begin, length);
                buffer_dst += length;
            }
        } else {
            if constexpr (is_last) {
                handled_write_buffer_stdout(buffered_size());
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

    template <bool is_first, bool is_last>
    [[gnu::always_inline]] void write (const std::string_view& msg) {
        return write_string_view<is_first, is_last>(msg.data(), msg.size());
    }
    template <bool is_first, bool is_last, size_t N>
    [[gnu::always_inline]] void write (const char (&value)[N]) {
        return write_string_view<is_first, is_last>(static_cast<const char*>(value), N - 1);
    }
    template <bool is_first, bool is_last, size_t N>
    [[gnu::always_inline]] void write (const StringLiteral<N>& value) {
        return write_string_view<is_first, is_last>(static_cast<const char*>(value.data), N);
    }
    template <bool is_first, bool is_last, typename T>
    requires (!trivially_loggable<std::remove_cvref_t<T>>)
    [[gnu::always_inline]] void write (const T& loggable) {
        loggable.log(writer<writer_params<is_first, is_last>>{*this});
    }

    template <bool is_first, bool is_last, char C>
    [[gnu::always_inline]] void write_char () {
        if constexpr (is_last) {
            if constexpr (is_first) {
                constexpr char char_buffer[1] {C};
                _handled_write_stdout(char_buffer, 1);
            } else {
                *(buffer_dst++) = C;
                handled_write_buffer_stdout(buffered_size());
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

    template <bool is_first, bool is_last, bool is_negative, std::unsigned_integral T>
    void write_nonzero (T value) {
        auto log10_of_value = fast_math::log_unsafe<10>(value);
        constexpr size_t sign_size = is_negative ? 1 : 0;
        if constexpr (is_negative) {
            *(buffer_dst++) = '-';
        }
        auto length = log10_of_value + 1;
        char* end; // End is only initialized if !is_first
        if constexpr (!is_first || buffer_size <= (ce::log10<std::numeric_limits<uint64_t>::max()> + sign_size)) {
            end = buffer_dst + length;
            if (end >= buffer_end) {
                handled_write_buffer_stdout(buffered_size());
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
                handled_write_buffer_stdout(gsl::narrow_cast<size_t>(end - buffer));
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

    template <bool is_first, bool is_last, std::unsigned_integral T>
    [[gnu::always_inline]] void write (T value) {
        if (value == 0) {
            write_char<is_first, is_last, '0'>();
        } else {
            write_nonzero<is_first, is_last, false>(value);
        }
    }

    template <bool is_first, bool is_last, std::signed_integral T>
    [[gnu::always_inline]] void write (T value) {
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
    [[gnu::always_inline]] void write (bool value) {
        if (value) {
            write<is_first, is_last>("true"_sl);
        } else {
            write<is_first, is_last>("false"_sl);
        }
    }

    template <StringLiteral value, bool has_buffered, bool buffered, bool no_newline>
    [[gnu::always_inline]] void _write_value () {
        if constexpr (no_newline) {
            write<!has_buffered, !buffered>(value);
        } else {
            write<!has_buffered, !buffered>(value + "\n"_sl);
        }
    }

    template <StringLiteral value, bool buffered, bool no_newline = false>
    [[gnu::always_inline]] void write_values () {
        if (buffer_dst == buffer) {
            _write_value<value, false, buffered, no_newline>();
        } else {
            _write_value<value, true, buffered, no_newline>();
        }
    }

    template <StringLiteral prefix, bool has_buffered, bool buffered, bool no_newline, typename... T>
    requires (sizeof...(T) > 0 && !no_newline)
    [[gnu::always_inline]] void _write_values (T&&... values) {
        write<!has_buffered, false>(std::move(prefix));
        (write<false, false>(std::forward<T>(values)), ...);
        write_char<false, !buffered, '\n'>();
    }

    template <bool buffered, typename FirstT, typename... RestT>
    [[gnu::always_inline]] void _write_rest_values (FirstT&& first, RestT&&... rest) {
        if constexpr (sizeof...(RestT) > 0) {
            write<false, false>(std::forward<FirstT>(first));
            _write_rest_values<buffered>(std::forward<RestT>(rest)...);
        } else {
            write<false, !buffered>(std::forward<FirstT>(first));
        }
    }

    template <StringLiteral prefix, bool has_buffered, bool buffered, bool no_newline, typename... T>
    requires (sizeof...(T) > 0 && no_newline)
    [[gnu::always_inline]] void _write_values (T&&... values) {
        write<!has_buffered, false>(std::move(prefix));
        _write_rest_values<buffered>(std::forward<T>(values)...);
    }

    template <StringLiteral prefix, bool buffered, bool no_newline = false, typename... T>
    requires (sizeof...(T) > 0)
    [[gnu::always_inline]] void write_values (T&&... values) {
        if (buffer_dst == buffer) {
            _write_values<prefix, false, buffered, no_newline>(std::forward<T>(values)...);
        } else {
            _write_values<prefix, true, buffered, no_newline>(std::forward<T>(values)...);
        }
    }

public:
    template <bool no_newline = false, bool buffered = false, StringLiteral first_value, typename... T>
    void log (T&&... values) {
        write_values<first_value, buffered, no_newline>(std::forward<T>(values)...);
    }
    template <bool no_newline = false, bool buffered = false, typename... T>
    void log (T&&... values) {
        write_values<"", buffered, no_newline>(std::forward<T>(values)...);
    }

    template <bool buffered = false, StringLiteral first_value, typename... T>
    void info (T&&... values) {
        write_values<info_prefix + first_value, buffered>(std::forward<T>(values)...);
    }
    template <bool buffered = false, typename... T>
    void info (T&&... values) {
        write_values<info_prefix, buffered>(std::forward<T>(values)...);
    }

    template <bool buffered = false, StringLiteral first_value, typename... T>
    void debug ([[maybe_unused]] T&&... values) {
        write_values<debug_prefix + first_value, buffered>(std::forward<T>(values)...);
    }
    template <bool buffered = false, typename... T>
    void debug ([[maybe_unused]] T&&... values) {
        write_values<debug_prefix, buffered>(std::forward<T>(values)...);
    }

    template <bool buffered = false, StringLiteral first_value, typename... T>
    void warn (T&&... values) {
        write_values<warn_prefix + first_value, buffered>(std::forward<T>(values)...);
    }
    template <bool buffered = false, typename... T>
    void warn (T&&... values) {
        write_values<warn_prefix, buffered>(std::forward<T>(values)...);
    }

    template <bool buffered = false, StringLiteral first_value, typename... T>
    void error (T&&... values) {
        write_values<error_prefix + first_value, buffered>(std::forward<T>(values)...);
    }
    template <bool buffered = false, typename... T>
    void error (T&&... values) {
        write_values<error_prefix, buffered>(std::forward<T>(values)...);
    }

    void flush () {
        if (buffer_dst == buffer) return;
        handled_write_buffer_stdout(buffered_size());
        buffer_dst = buffer;
    }
};




template <typename T>
concept custom_loggable = requires (T t) {
    { t.log(std::declval<logger::writer<logger::writer_params<false, false>>>()) } -> std::same_as<void>;
    { t.log(std::declval<logger::writer<logger::writer_params<false, true >>>()) } -> std::same_as<void>;
    { t.log(std::declval<logger::writer<logger::writer_params<true , false>>>()) } -> std::same_as<void>;
    { t.log(std::declval<logger::writer<logger::writer_params<true , true >>>()) } -> std::same_as<void>;
};


template <typename T>
concept loggable = trivially_loggable<T> || custom_loggable<T>;

static logger console {"/dev/stdout"}; // TODO Static might cause probelms when switching to multiple TUs

template <StringLiteral auto_msg, typename... ArgsT>
[[noreturn, gnu::noinline, gnu::cold]] void bssert_fail (ArgsT&&... args) {
    if constexpr (sizeof...(ArgsT) > 0) {
        console.error<false, auto_msg + " with "_sl>(std::forward<ArgsT>(args)...);
    } else {
        console.error<false, auto_msg>();
    }
    __builtin_trap();
}

#define BSSERT(EXPR, ...)                                                                                       \
/* NOLINTNEXTLINE(readability-simplify-boolean-expr) */                                                         \
if (!(EXPR)) {                                                                                                  \
    bssert_fail<"Assertion `" #EXPR "` at " __FILE__ ":" BOOST_PP_STRINGIZE(__LINE__) " failed">(__VA_ARGS__);  \
}

template<StringLiteral auto_msg, StringLiteral op, typename T, typename U, typename... ArgsT>
[[noreturn, gnu::noinline, gnu::cold]] void cssert_fail (T&& lhs, U&& rhs, ArgsT&&... args) {
    console.log<true, true, logger::error_prefix + auto_msg>();
    if constexpr (loggable<std::remove_cvref_t<T>>) {
        console.log<true, true>(std::forward<T>(lhs), op);
    } else {
        static constexpr auto lhs_type_name = string_literal::from_([](){ return nameof::nameof_type<T>(); });
        console.log<true, true, string_literal::concat_v<lhs_type_name, "{?}"_sl, op>>();
    }
    if constexpr (loggable<std::remove_cvref_t<U>>) {
        if constexpr (sizeof...(ArgsT) > 0) {
            console.log<false, false>(std::forward<U>(rhs), "` and ", std::forward<ArgsT>(args)...);
        } else {
            console.log<true, false>(std::forward<U>(rhs), "`\n");
        }
    } else {
        static constexpr auto rhs_type_name = string_literal::from_([](){ return nameof::nameof_type<U>(); });
        if constexpr (sizeof...(ArgsT) > 0) {
            console.log<false, false, rhs_type_name + "{?}` and "_sl>(std::forward<ArgsT>(args)...);
        } else {
            console.log<true, false, rhs_type_name + "{?}`\n"_sl>();
        }
    }
    __builtin_trap();
}

#define CSSERT(LHS, OP, RHS, ...)                                                                                                                                           \
/* NOLINTNEXTLINE(readability-simplify-boolean-expr) */                                                                                                                     \
if (!(LHS OP RHS)) {                                                                                                                                                        \
    cssert_fail<"Assertion `" #LHS " " #OP " " #RHS "` at " __FILE__ ":" BOOST_PP_STRINGIZE(__LINE__) " failed with `", " " #OP " ">(LHS, RHS __VA_OPT__(,) __VA_ARGS__);   \
}
