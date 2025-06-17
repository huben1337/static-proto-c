#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <minwindef.h>
#include <string_view>
#include <synchapi.h>
#include <tuple>
#include <type_traits>
#include <utility>
#include <io.h>
#include "io.cpp"

#if defined(__MINGW64__) || defined(__MINGW32__)

#include <windows.h>

#endif

#include "base.cpp"
#include "string_literal.cpp"
#include "fast_math.cpp"

#include <boost/preprocessor/stringize.hpp>
#include <winnt.h>

#define BSSERT(EXPR, MSG, MORE...)                                                                                                          \
{                                                                                                                                           \
    if (!(EXPR)) {                                                                                                                          \
        [[clang::noinline]] logger::error<"Assertion " #EXPR " failed at " __FILE__ ":" BOOST_PP_STRINGIZE(__LINE__) "with " MSG>(MORE);    \
        exit(1);                                                                                                                            \
    }                                                                                                                                       \
}

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
                "2;"_sl + uint_to_string<closest_cube6<r>>()
                + ";"_sl + uint_to_string<closest_cube6<g>>()
                + ";"_sl + uint_to_string<closest_cube6<b>>();

                template <uint8_t v>
                requires(v < 24)
                constexpr auto _gray_scale = "2;"_sl + uint_to_string<232 + v>();
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

#define ASYNC_STDOUT

namespace _logger_internal_namespace {
    static constexpr size_t buffer_size = 1 << 20;
    static constexpr size_t circular_buffer_count = 2; // Two is the minimum to enable async writes. More than two is uselss since we will wait for the previous write to finish anyways
    alignas(4096) static char _buffer[buffer_size * circular_buffer_count];

    static size_t buffer_offset = 0;

    static char* buffer = _buffer;
    static char* buffer_end = buffer + buffer_size;
    static char* buffer_dst = buffer;
    

    #ifdef _WINDOWS_

    #ifdef ASYNC_STDOUT
    #define FILE_FLAGS_AND_ATTRIBUTES FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING
    #else
    #define FILE_FLAGS_AND_ATTRIBUTES 0
    #endif

    static const io::windows::FileHandle<
        io::windows::CreateFileParamsWithName<
            "CONOUT$",
            GENERIC_WRITE,
            FILE_SHARE_WRITE,
            OPEN_EXISTING,
            FILE_FLAGS_AND_ATTRIBUTES
        >
    > stdout_handle {NULL, NULL};

    static io::windows::AsyncFileWriter stdout_writer {stdout_handle};


    #undef FILE_FLAGS_AND_ATTRIBUTES

        
    #endif


    class logger {
        private:
        logger () = delete;
        ~logger () = delete;

    
        template <StringLiteral name, StringLiteral color_code>
        static constexpr auto LOG_LEVEL = "\033["_sl + color_code + "m["_sl + name + "]\033[0m "_sl;
    
        static constexpr auto debug_prefix = LOG_LEVEL<"DEBUG", escape_sequences::colors::bright::foreground::yellow>;
        static constexpr auto info_prefix = LOG_LEVEL<"INFO", escape_sequences::colors::bright::foreground::green>;
        static constexpr auto error_prefix = LOG_LEVEL<"ERROR", escape_sequences::colors::bright::foreground::red>;
        static constexpr auto warn_prefix = LOG_LEVEL<"WARN", escape_sequences::colors::bright::foreground::magenta>;

        
        static INLINE void handled_write_buffer_stdout (size_t size) {
            _handled_write_stdout(buffer, size);
            buffer_offset = (buffer_offset + buffer_size) % (buffer_size * circular_buffer_count);
            buffer = _buffer + buffer_offset;
            buffer_end = buffer + buffer_size;
        }

        static INLINE void _handled_write_stdout (const char* src, size_t size) {
            #ifdef _WINDOWS_
            stdout_writer.write_handled(src, size);

            #else
            int write_result = ::write(STDOUT_FILENO, src, size);
            if (write_result == -1) [[unlikely]] {
                static constexpr auto error_msg = "\n"_sl + error_prefix + "[logger::_handled_write_stdout] Failed to write to stdout."_sl;
                ::write(STDOUT_FILENO, error_msg.value, error_msg.size());
                exit(1);
            }
            #endif
        }

        template<bool is_first, bool is_last>
        static INLINE void write (const char* begin, size_t length) {
            if constexpr (is_first) {
                if constexpr (is_last) {
                    _handled_write_stdout(begin, length);
                } else {
                    if (length >= buffer_size) {
                        _handled_write_stdout(begin, length);
                    } else {
                        memcpy(buffer, begin, length);
                        buffer_dst += length;
                    }
                }
            } else /* if constexpr (!is_first) */ {
                size_t free_space = buffer_end - buffer_dst;
                if (free_space >= length) {
                    if constexpr (is_last) {
                        memcpy(buffer_dst, begin, length);
                        handled_write_buffer_stdout(buffer_dst + length - buffer);
                        buffer_dst = buffer;
                    } else /* if constexpr (!is_last) */ {
                        memcpy(buffer_dst, begin, length);
                        buffer_dst += length;
                    }
                } else {
                    const char* end = begin + length;
                    memcpy(buffer_dst, begin, free_space);
                    handled_write_buffer_stdout(buffer_size);
                    begin += free_space;
                    while (begin < end - buffer_size) {
                        memcpy(buffer, begin, buffer_size);
                        handled_write_buffer_stdout(buffer_size);
                        begin += buffer_size;
                    }
                    if (begin < end - 1) {
                        size_t remaining = end - begin;
                        if constexpr (is_last) {
                            _handled_write_stdout(begin, remaining);
                            buffer_dst = buffer;
                        } else {
                            memcpy(buffer, begin, remaining);
                            buffer_dst = buffer + remaining;
                        }               
                    } else {
                        buffer_dst = buffer;
                    }
                }
            }
        }
    
        template<bool is_first, bool is_last>
        static INLINE void write (const std::string_view& msg) {
            return write<is_first, is_last>(msg.begin(), msg.size());
        }
        template<bool is_first, bool is_last>
        static INLINE void write (std::string_view&& msg) {
            return write<is_first, is_last>(msg.begin(), msg.size());
        }
        template <bool is_first, bool is_last, size_t N>
        static INLINE void write (const char (&value)[N]) {
            return write<is_first, is_last>(value, N - 1);
        }
        template <bool is_first, bool is_last, size_t N>
        static INLINE void write (char (&&value)[N]) {
            return write<is_first, is_last>(value, N - 1);
        }
        template <bool is_first, bool is_last, size_t N>
        static INLINE void write (const StringLiteral<N>& value) {
            return write<is_first, is_last>(value.value, N - 1);
        }
        template <bool is_first, bool is_last, size_t N>
        static INLINE void write (StringLiteral<N>&& value) {
            return write<is_first, is_last>(value.value, N - 1);
        }
        template<bool is_first, bool is_last, bool is_negative, fast_math::uint64or32_c T>
        static INLINE void write_nonzero (T value) {
            const uint8_t log10_of_value = fast_math::log10_unsafe(value);
            constexpr size_t sign_size = is_negative ? 1 : 0;
            if constexpr (is_negative) {
                *(buffer_dst++) = '-';
            }
            const size_t length = log10_of_value + 1;
            char* end; // End is only initialized if !is_first
            if constexpr (!is_first || buffer_size <= (uint_log10<std::numeric_limits<uint64_t>::max()> + sign_size)) {
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
        template<bool is_first, bool is_last, char C>
        static INLINE void write_char () {
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
        template <std::integral T>
        using fitting_u_int64_32 = std::conditional_t<
            std::is_signed_v<T>,
            std::conditional_t<sizeof(T) == sizeof(int64_t), int64_t, int32_t>,
            std::conditional_t<sizeof(T) == sizeof(uint64_t), uint64_t, uint32_t>
        >;
        
        template<bool is_first, bool is_last, fast_math::uint64or32_c T>
        static INLINE void write_uint (T value) {
            if (value == 0) {
                write_char<is_first, is_last, '0'>();
            } else {
                write_nonzero<is_first, is_last, false>(value);
            }
        }
        template <bool is_first, bool is_last>
        static INLINE void write (uint64_t value) {
            write_uint<is_first, is_last>(value);
        }
        template <bool is_first, bool is_last>
        static INLINE void write (unsigned long value) {
            write_uint<is_first, is_last>(static_cast<fitting_u_int64_32<unsigned long>>(value));
        }
        template <bool is_first, bool is_last>
        static INLINE void write (uint32_t value) {
            write_uint<is_first, is_last>(value);
        }
        template <bool is_first, bool is_last>
        static INLINE void write (uint16_t value) {
            write_uint<is_first, is_last>(static_cast<uint32_t>(value));
        }
        template <bool is_first, bool is_last>
        static INLINE void write (uint8_t value) {
            write_uint<is_first, is_last>(static_cast<uint32_t>(value));
        }
        template<bool is_first, bool is_last, fast_math::int64or32_c T>
        static INLINE void write_int (T value) {
            if (value == 0) {
                write_char<is_first, is_last, '0'>();
            } else if (value < 0) {
                write_nonzero<false, is_last, true>(static_cast<std::make_unsigned_t<T>>(-value));
            } else {
                write_nonzero<is_first, is_last, false>(static_cast<std::make_unsigned_t<T>>(value));
            }
        }
        template <bool is_first, bool is_last>
        static INLINE void write (int64_t value) {
            write_int<is_first, is_last>(value);
        }
        template <bool is_first, bool is_last>
        static INLINE void write (long value) {
            write_int<is_first, is_last>(static_cast<fitting_u_int64_32<long>>(value));
        }
        template <bool is_first, bool is_last>
        static INLINE void write (int32_t value) {
            write_int<is_first, is_last>(value);
        }
        template <bool is_first, bool is_last>
        static INLINE void write (int16_t value) {
            write_int<is_first, is_last>(static_cast<int32_t>(value));
        }
        template <bool is_first, bool is_last>
        static INLINE void write (int8_t value) {
            write_int<is_first, is_last>(static_cast<int32_t>(value));
        }
    
        template <typename... T, size_t... Indecies>
        requires (sizeof...(T) > 0)
        static INLINE void write_tuple (std::tuple<T...> values, std::index_sequence<Indecies...>) {
            (write<false, false>(std::forward<std::tuple_element_t<Indecies, std::tuple<T...>>>(std::get<Indecies>(values))), ...);
        }
    
        template <StringLiteral value, bool has_buffered, bool buffered>
        static INLINE void _write_value () {
            write<!has_buffered, !buffered>(value + "\n"_sl);
        }

        template <StringLiteral value, bool buffered>
        static INLINE void write_values () {
            if (buffer_dst == buffer) {
                _write_value<value, false, buffered>();
            } else {
                _write_value<value, true, buffered>();
            }
        }
    
        template <StringLiteral prefix, bool has_buffered, bool buffered, bool no_newline, typename... T>
        requires (sizeof...(T) > 0 && no_newline == false)
        static INLINE void _write_values (T&&... values) {
            write<!has_buffered, false>(std::move(prefix));
            (write<false, false>(std::forward<T>(values)), ...);
            write_char<false, !buffered, '\n'>();
        }

        

        template <bool buffered, typename FirstT, typename ...RestT>
        static INLINE void _write_rest_values (FirstT&& first, RestT&&... rest) {
            if constexpr (sizeof...(RestT) > 0) {
                write<false, false>(std::forward<FirstT>(first));
                _write_rest_values<buffered>(std::forward<RestT>(rest)...);
            } else {
                write<false, !buffered>(std::forward<FirstT>(first));
            }
        }

        template <StringLiteral prefix, bool has_buffered, bool buffered, bool no_newline, typename... T>
        requires (sizeof...(T) > 0 && no_newline == true)
        static INLINE void _write_values (T&&... values) {
            write<!has_buffered, false>(std::move(prefix));
            _write_rest_values<buffered>(std::forward<T>(values)...);
        }

        template <StringLiteral prefix, bool buffered, bool no_newline = false, typename... T>
        requires (sizeof...(T) > 0)
        static INLINE void write_values (T&&... values) {
            if (buffer_dst == buffer) {
                _write_values<prefix, false, buffered, no_newline>(std::forward<T>(values)...);
            } else {
                _write_values<prefix, true, buffered, no_newline>(std::forward<T>(values)...);
            }
        }
        
        public:

        template <bool no_newline = false, bool buffered = false, StringLiteral first_value, typename... T>
        static INLINE void log (T&&... values) {
            write_values<first_value, buffered, no_newline>(std::forward<T>(values)...);
        }
        template <bool no_newline = false, bool buffered = false,  typename... T>
        static INLINE void log (T&&... values) {
            write_values<"", buffered, no_newline>(std::forward<T>(values)...);
        }
    
        template <bool buffered = false, StringLiteral first_value, typename... T>
        static INLINE void info (T&&... values) {
            write_values<info_prefix + first_value, buffered>(std::forward<T>(values)...);
        }
        template <bool buffered = false, typename... T>
        static INLINE void info (T&&... values) {
            write_values<info_prefix, buffered>(std::forward<T>(values)...);
        }
    
        template <bool buffered = false, StringLiteral first_value, typename... T>
        static INLINE void debug (T&&... values) {
            //write_values<debug_prefix + first_value, buffered>(std::forward<T>(values)...);
        }
        template <bool buffered = false, typename... T>
        static INLINE void debug (T&&... values) {
            //write_values<debug_prefix, buffered>(std::forward<T>(values)...);
        }
    
        template <bool buffered = false, StringLiteral first_value, typename... T>
        static INLINE void warn (T&&... values) {
            write_values<warn_prefix + first_value, buffered>(std::forward<T>(values)...);
        }
        template <bool buffered = false, typename... T>
        static INLINE void warn (T&&... values) {
            write_values<warn_prefix, buffered>(std::forward<T>(values)...);
        }
        
        template <bool buffered = false, StringLiteral first_value, typename... T>
        static INLINE void error (T&&... values) {
            write_values<error_prefix + first_value, buffered>(std::forward<T>(values)...);
        }
        template <bool buffered = false, typename... T>
        static INLINE void error (T&&... values) {
            write_values<error_prefix, buffered>(std::forward<T>(values)...);
        }

        static void flush () {
            if (buffer_dst == buffer) return;
            handled_write_buffer_stdout(buffer_dst - buffer);
            buffer_dst = buffer;
        }
    };
};

using logger = _logger_internal_namespace::logger;

#undef ASYNC_STDOUT
