#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <io.h>
#include "base.cpp"
#include "string_literal.cpp"
#include "fast_math.cpp"

#include <boost/preprocessor/stringize.hpp>

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

namespace _logger_internal_namespace {
    alignas(4096) static char buffer[4096];
    static constexpr char* buffer_end = buffer + sizeof(buffer);

    class logger {
        logger () = delete;
        ~logger () = delete;

        private:
    
        template <StringLiteral name, StringLiteral color_code>
        static constexpr auto LOG_LEVEL = "\033["_sl + color_code + "m["_sl + name + "]\033[0m "_sl;
    
        static constexpr auto debug_prefix = LOG_LEVEL<"DEBUG", escape_sequences::colors::bright::foreground::yellow>;
        static constexpr auto info_prefix = LOG_LEVEL<"INFO", escape_sequences::colors::bright::foreground::green>;
        static constexpr auto error_prefix = LOG_LEVEL<"ERROR", escape_sequences::colors::bright::foreground::red>;
        static constexpr auto warn_prefix = LOG_LEVEL<"WARN", escape_sequences::colors::bright::foreground::magenta>;
    
        static INLINE void handled_write_stdout (const char* src, size_t size) {
            int result = ::write(STDOUT_FILENO, src, size);
            if (result < 0) {
                constexpr auto error_msg = error_prefix + "[logger::handled_write_stdout] Failed to write to stdout."_sl;
                ::write(STDOUT_FILENO, error_msg.value, error_msg.size());
                exit(1);
            }
        }
    
        /* INLINE char* copy_128 (const char* begin, const char* end, size_t length, char* dst) {
        uint8_t src_addr_trailing_zeros = __builtin_ctz(dst - buffer);
        #pragma clang switch vectorize(enable)
        switch (src_addr_trailing_zeros) {
            default: {
                #pragma clang loop vectorize(enable)
                while (begin < end - sizeof(__uint128_t)) {
                    reinterpret_cast<__uint128_t*>(dst++)[0] = reinterpret_cast<const __uint128_t*>(begin++)[0];
                }
            }
            case 3: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(__uint128_t)) {
                    reinterpret_cast<__uint128_t*>(dst++)[0] = static_cast<__uint128_t>(reinterpret_cast<const uint64_t*>(begin)[0]) | static_cast<__uint128_t>(reinterpret_cast<const uint64_t*>(begin + 1)[0]) << 64;
                    begin += 2;
                }
            }
            case 2: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(__uint128_t)) {
                    reinterpret_cast<__uint128_t*>(dst++)[0] = 
                    static_cast<__uint128_t>(reinterpret_cast<const uint32_t*>(begin)[0])
                    | static_cast<__uint128_t>(reinterpret_cast<const uint32_t*>(begin + 1)[0]) << 32
                    | static_cast<__uint128_t>(reinterpret_cast<const uint32_t*>(begin + 2)[0]) << 64
                    | static_cast<__uint128_t>(reinterpret_cast<const uint32_t*>(begin + 3)[0]) << 96;
                    begin += 4;
                }
            }
            case 1: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(__uint128_t)) {
                    reinterpret_cast<__uint128_t*>(dst++)[0] = 
                    static_cast<__uint128_t>(reinterpret_cast<const uint16_t*>(begin)[0])
                    | static_cast<__uint128_t>(reinterpret_cast<const uint16_t*>(begin + 1)[0]) << 16
                    | static_cast<__uint128_t>(reinterpret_cast<const uint16_t*>(begin + 2)[0]) << 32
                    | static_cast<__uint128_t>(reinterpret_cast<const uint16_t*>(begin + 3)[0]) << 48
                    | static_cast<__uint128_t>(reinterpret_cast<const uint16_t*>(begin + 4)[0]) << 64
                    | static_cast<__uint128_t>(reinterpret_cast<const uint16_t*>(begin + 5)[0]) << 80
                    | static_cast<__uint128_t>(reinterpret_cast<const uint16_t*>(begin + 6)[0]) << 96
                    | static_cast<__uint128_t>(reinterpret_cast<const uint16_t*>(begin + 7)[0]) << 112;
                    begin += 8;
                }
                size_t remaining = end - begin;
                memcpy(dst, begin, remaining);
                return dst + remaining;
            }
            case 0: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(__uint128_t)) {
                    reinterpret_cast<__uint128_t*>(dst++)[0] = 
                    static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin)[0])
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 1)[0]) << 8
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 2)[0]) << 16
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 3)[0]) << 24
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 4)[0]) << 32
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 5)[0]) << 40
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 6)[0]) << 48
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 7)[0]) << 56
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 8)[0]) << 64
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 9)[0]) << 72
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 10)[0]) << 80
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 11)[0]) << 88
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 12)[0]) << 96
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 13)[0]) << 104
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 14)[0]) << 112
                    | static_cast<__uint128_t>(reinterpret_cast<const uint8_t*>(begin + 15)[0]) << 120;
                    begin += 16;
                }
                size_t remaining = end - begin;
            }
        }
    }
    
    INLINE char* copy_64 (const char* begin, const char* end, size_t length, char* dst) {
        uint8_t src_addr_trailing_zeros = __builtin_ctz(dst - buffer);
        switch (src_addr_trailing_zeros) {
            default: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(uint64_t)) {
                    reinterpret_cast<uint64_t*>(dst++)[0] = reinterpret_cast<const uint64_t*>(begin++)[0];
                }
            }
            case 2: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(uint64_t)) {
                    reinterpret_cast<uint64_t*>(dst++)[0] =
                    static_cast<uint64_t>(reinterpret_cast<const uint32_t*>(begin)[0])
                    | static_cast<uint64_t>(reinterpret_cast<const uint32_t*>(begin + 1)[0]) << 32;
                    begin += 2;
                }
            }
            case 1: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(uint64_t)) {
                    reinterpret_cast<uint64_t*>(dst++)[0] =
                    static_cast<uint64_t>(reinterpret_cast<const uint16_t*>(begin)[0])
                    | static_cast<uint64_t>(reinterpret_cast<const uint16_t*>(begin + 1)[0]) << 16
                    | static_cast<uint64_t>(reinterpret_cast<const uint16_t*>(begin + 2)[0]) << 32
                    | static_cast<uint64_t>(reinterpret_cast<const uint16_t*>(begin + 3)[0]) << 48;
                    begin += 4;
                }
            }
            case 0: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(uint64_t)) {
                    reinterpret_cast<uint64_t*>(dst++)[0] =
                    static_cast<uint64_t>(reinterpret_cast<const uint8_t*>(begin)[0])
                    | static_cast<uint64_t>(reinterpret_cast<const uint8_t*>(begin + 1)[0]) << 8
                    | static_cast<uint64_t>(reinterpret_cast<const uint8_t*>(begin + 2)[0]) << 16
                    | static_cast<uint64_t>(reinterpret_cast<const uint8_t*>(begin + 3)[0]) << 24
                    | static_cast<uint64_t>(reinterpret_cast<const uint8_t*>(begin + 4)[0]) << 32
                    | static_cast<uint64_t>(reinterpret_cast<const uint8_t*>(begin + 5)[0]) << 40
                    | static_cast<uint64_t>(reinterpret_cast<const uint8_t*>(begin + 6)[0]) << 48
                    | static_cast<uint64_t>(reinterpret_cast<const uint8_t*>(begin + 7)[0]) << 56;
                    begin += 8;
                }
            }
        }
    }
    
    INLINE char* copy_32 (const char* begin, const char* end, size_t length, char* dst) {
        uint8_t src_addr_trailing_zeros = __builtin_ctz(dst - buffer);
        switch (src_addr_trailing_zeros) {
            default: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(uint32_t)) {
                    reinterpret_cast<uint32_t*>(dst++)[0] = reinterpret_cast<const uint32_t*>(begin++)[0];
                }
                case 1: {
                    #pragma clang loop vectorize(enable)
                    while (begin <= end - sizeof(uint32_t)) {
                        reinterpret_cast<uint32_t*>(dst++)[0] =
                        static_cast<uint32_t>(reinterpret_cast<const uint16_t*>(begin)[0])
                        | static_cast<uint32_t>(reinterpret_cast<const uint16_t*>(begin + 1)[0]) << 16;
                        begin += 2;
                    }
                }
                case 0: {
                    #pragma clang loop vectorize(enable)
                    while (begin <= end - sizeof(uint32_t)) {
                        reinterpret_cast<uint32_t*>(dst++)[0] =
                        static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(begin)[0])
                        | static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(begin + 1)[0]) << 8
                        | static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(begin + 2)[0]) << 16
                        | static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(begin + 3)[0]) << 24;
                        begin += 4;
                    }
                }
            }
        }
    }
    
    INLINE char* copy_16 (const char* begin, const char* end, size_t length, char* dst) {
        uint8_t src_addr_trailing_zeros = __builtin_ctz(dst - buffer);
        switch (src_addr_trailing_zeros) {
            default: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(uint16_t)) {
                    reinterpret_cast<uint16_t*>(dst++)[0] = reinterpret_cast<const uint16_t*>(begin++)[0];
                }
            }
            case 0: {
                #pragma clang loop vectorize(enable)
                while (begin <= end - sizeof(uint16_t)) {
                    reinterpret_cast<uint16_t*>(dst++)[0] =
                    static_cast<uint16_t>(reinterpret_cast<const uint8_t*>(begin)[0])
                    | static_cast<uint16_t>(reinterpret_cast<const uint8_t*>(begin + 1)[0]) << 8;
                    begin += 2;
                }
            }
        }
    }
    
    INLINE char* bmem_copy (const char* begin, const char* end, size_t length, char* dst) {
        uint8_t src_addr_trailing_zeros = __builtin_ctz(dst - buffer);
        uint8_t dst_addr_trailing_zeros = __builtin_ctz(buffer_end - dst);
        uint8_t min_trailing_zeros = std::min(src_addr_trailing_zeros, dst_addr_trailing_zeros);
        switch (dst_addr_trailing_zeros) {
            default: {
                // 128
                
            }
            case 3: {
                // 64
                
            }
            case 2: {
                // 32
                
            }
            case 1: {
                // 16
            }
            case 0: {
                // 8
            }
        }
        } */
    
        template<bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (const char* begin, const char* end, size_t length, char* dst) {
            size_t free_space = buffer_end - dst;
            if (free_space >= length) {
                if constexpr (is_last) {
                    if constexpr (is_first) {
                        handled_write_stdout(begin, length);
                    } else {
                        memcpy(dst, begin, length);
                        handled_write_stdout(buffer, dst + length - buffer);
                    }
                } else {
                    memcpy(dst, begin, length);
                    return dst + length;
                }
            } else {
                memcpy(dst, begin, free_space);
                handled_write_stdout(buffer, sizeof(buffer));
                begin += free_space;
                while (begin < end - sizeof(buffer)) {
                    memcpy(buffer, begin, sizeof(buffer));
                    handled_write_stdout(buffer, sizeof(buffer));
                    begin += sizeof(buffer);
                }
                if (begin < end - 1) {
                    size_t remaining = end - begin;
                    if constexpr (is_last) {
                        handled_write_stdout(begin, remaining);
                    } else {
                        memcpy(buffer, begin, remaining);
                        return buffer + remaining;
                    }               
                } else {
                    if constexpr (!is_last) {
                        return buffer + 0;
                    }
                }
            }
        }
    
        template<bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, const std::string_view& msg) {
            return write<is_first, is_last>(msg.begin(), msg.end(), msg.size(), dst);
        }
        template<bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, const std::string_view&& msg) {
            return write<is_first, is_last>(msg.begin(), msg.end(), msg.size(), dst);
        }
        template <bool is_first, bool is_last, size_t N>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, const char (&value)[N]) {
            return write<is_first, is_last>(value, value + N - 1, N - 1, dst);
        }
        template <bool is_first, bool is_last, size_t N>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, const char (&&value)[N]) {
            return write<is_first, is_last>(value, value + N - 1, N - 1, dst);
        }
        template <bool is_first, bool is_last, size_t N>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, const StringLiteral<N>&& value) {
            return write<is_first, is_last>(value.value, value.value + N - 1, N - 1, dst);
        }
        template<bool is_first, bool is_last, bool is_negative, fast_math::uint64or32_c T>
        static INLINE std::conditional_t<is_last, void, char*> write_nonzero (char* dst, T value) {
            const uint8_t i = fast_math::log10_unsafe(value);
            constexpr size_t sign_size = is_negative ? 1 : 0;
            if (is_negative) {
                *(dst++) = '-';
            }
            char* const end = dst + i + 1;
            if constexpr (!is_first || sizeof(buffer) <= (uint_log10<std::numeric_limits<uint64_t>::max()> + sign_size)) {
                if (end >= buffer_end) {
                    handled_write_stdout(buffer, dst - buffer);
                    dst = buffer;
                }
            }
            dst += i;
            *(dst--) = '0' + (value % 10);
            value /= 10;
            while (value > 0) {
                *(dst--) = '0' + (value % 10);
                value /= 10;
            }
            if constexpr (is_last) {
                handled_write_stdout(buffer, end - buffer);
            } else {
                return end;
            }
        }
        template<bool is_first, bool is_last, char C>
        static INLINE std::conditional_t<is_last, void, char*> write_char (char* dst) {
            *(dst++) = C;
            if constexpr (is_last) {
                if constexpr (is_first) {
                    handled_write_stdout(buffer, 1);
                } else {
                    handled_write_stdout(buffer, dst - buffer);
                }
            } else {
                if constexpr (is_first && sizeof(buffer) > 1) {
                    return dst;
                }
                if (dst == buffer_end) {
                    handled_write_stdout(buffer, sizeof(buffer));
                    return 0;
                }
                return dst;
            }
        }
        template <std::integral T>
        using fitting_u_int64_32 = std::conditional_t<
            std::is_signed_v<T>,
            std::conditional_t<sizeof(T) == sizeof(int64_t), int64_t, int32_t>,
            std::conditional_t<sizeof(T) == sizeof(uint64_t), uint64_t, uint32_t>
            >;
        template<bool is_first, bool is_last, fast_math::uint64or32_c T>
        static INLINE std::conditional_t<is_last, void, char*> write_uint (char* dst, T value) {
            if (value == 0) {
                return write_char<is_first, is_last, '0'>(dst);
            } else {
                return write_nonzero<is_first, is_last, false>(dst, value);
            }
        }
        template <bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, uint64_t value) {
            return write_uint<is_first, is_last>(dst, value);
        }
        template <bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, unsigned long value) {
            return write_uint<is_first, is_last>(dst, static_cast<fitting_u_int64_32<unsigned long>>(value));
        }
        template <bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, uint32_t value) {
            return write_uint<is_first, is_last>(dst, value);
        }
        template <bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, uint16_t value) {
            return write_uint<is_first, is_last>(dst, static_cast<uint32_t>(value));
        }
        template <bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, uint8_t value) {
            return write_uint<is_first, is_last>(dst, static_cast<uint32_t>(value));
        }
        template<bool is_first, bool is_last, fast_math::int64or32_c T>
        static INLINE std::conditional_t<is_last, void, char*> write_int (char* dst, T value) {
            if (value == 0) {
                return write_char<is_first, is_last, '0'>(dst);
            } else if (value < 0) {
                return write_nonzero<false, is_last, true>(dst, static_cast<std::make_unsigned_t<T>>(-value));
            } else {
                return write_nonzero<is_first, is_last, false>(dst, static_cast<std::make_unsigned_t<T>>(value));
            }
        }
        template <bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, int64_t value) {
            return write_int<is_first, is_last>(dst, value);
        }
        template <bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, long value) {
            return write_int<is_first, is_last>(dst, static_cast<fitting_u_int64_32<long>>(value));
        }
        template <bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, int32_t value) {
            return write_int<is_first, is_last>(dst, value);
        }
        template <bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, int16_t value) {
            return write_int<is_first, is_last>(dst, static_cast<int32_t>(value));
        }
        template <bool is_first, bool is_last>
        static INLINE std::conditional_t<is_last, void, char*> write (char* dst, int8_t value) {
            return write_int<is_first, is_last>(dst, static_cast<int32_t>(value));
        }
    
        template <typename... T, size_t... Indecies>
        requires (sizeof...(T) > 0)
        static INLINE char* write_tuple (char* dst, std::tuple<T...> values, std::index_sequence<Indecies...>) {
            ((
                dst = write<false, false>(dst, std::forward<std::tuple_element_t<Indecies, std::tuple<T...>>>(std::get<Indecies>(values)))
            ), ...);
            return dst;
        }
    
        template <StringLiteral value>
        static INLINE void write_values () {
            write<true, true>(buffer, value + "\n"_sl);
        }
    
        template <StringLiteral prefix, typename... T>
        requires (sizeof...(T) > 0)
        static INLINE void write_values (T&&... values) {
            char* dst = write<true, false>(buffer, std::move(prefix));
            dst = write_tuple(dst, std::forward_as_tuple<T...>(std::forward<T>(values)...), std::make_index_sequence<sizeof...(T)>{});
            write<false, true>(dst, "\n"_sl);
        }
        
        public:

        template <StringLiteral first_value, typename... T>
        static INLINE void log (T&&... values) {
            write_values<first_value>(std::forward<T>(values)...);
        }
        template <typename... T>
        static INLINE void log (T&&... values) {
            write_values<"">(std::forward<T>(values)...);
        }
    
        template <StringLiteral first_value, typename... T>
        static INLINE void info (T&&... values) {
            write_values<info_prefix + first_value>(std::forward<T>(values)...);
        }
        template <typename... T>
        static INLINE void info (T&&... values) {
            write_values<info_prefix>(std::forward<T>(values)...);
        }
    
        template <StringLiteral first_value, typename... T>
        static INLINE void debug (T&&... values) {
            write_values<debug_prefix + first_value>(std::forward<T>(values)...);
        }
        template <typename... T>
        static INLINE void debug (T&&... values) {
            // write_values<debug_prefix>(std::forward<T>(values)...);
        }
    
        template <StringLiteral first_value, typename... T>
        static INLINE void warn (T&&... values) {
            write_values<warn_prefix + first_value>(std::forward<T>(values)...);
        }
        template <typename... T>
        static INLINE void warn (T&&... values) {
            write_values<warn_prefix>(std::forward<T>(values)...);
        }
        
        template <StringLiteral first_value, typename... T>
        static INLINE void error (T&&... values) {
            write_values<error_prefix + first_value>(std::forward<T>(values)...);
        }
        template <typename... T>
        static INLINE void error (T&&... values) {
            write_values<error_prefix>(std::forward<T>(values)...);
        }
    };
};

using logger = _logger_internal_namespace::logger;