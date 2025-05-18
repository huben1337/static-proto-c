#pragma once
#include "base.cpp"
#include <cstdint>
#include <cstdlib>
#include <stdio.h>
#include <io.h>
#include <type_traits>

namespace fast_math {

    template<typename T>
    concept uint64or32_c = std::is_same_v<T, uint64_t> || std::is_same_v<T, uint32_t>;
    template<typename T>
    concept int64or32_c = std::is_same_v<T, int64_t> || std::is_same_v<T, int32_t>;

    INLINE constexpr int _clz (uint32_t x) { return __builtin_clz(x); }
    INLINE constexpr int _clz (uint64_t x) { return __builtin_clzll(x); }

    namespace log10_impl_1 {
        INLINE constexpr uint64_t get_pow10_table (uint8_t n) {
            static constexpr uint64_t pow10[19] = {
                10ULL,
                100ULL,
                1000ULL,
                10000ULL,
                100000ULL,
                1000000ULL,
                10000000ULL,
                100000000ULL,
                1000000000ULL,
                10000000000ULL,
                100000000000ULL,
                1000000000000ULL,
                10000000000000ULL,
                100000000000000ULL,
                1000000000000000ULL,
                10000000000000000ULL,
                100000000000000000ULL,
                1000000000000000000ULL,
                10000000000000000000ULL
            };
            return pow10[n];
        }

        INLINE constexpr uint64_t get_pow10_switch (uint8_t n) {
            switch (n) {
                case 0:     return 10ULL;
                case 1:     return 100ULL;
                case 2:     return 1000ULL;
                case 3:     return 10000ULL;
                case 4:     return 100000ULL;
                case 5:     return 1000000ULL;
                case 6:     return 10000000ULL;
                case 7:     return 100000000ULL;
                case 8:     return 1000000000ULL;
                case 9:     return 10000000000ULL;
                case 10:    return 100000000000ULL;
                case 11:    return 1000000000000ULL;
                case 12:    return 10000000000000ULL;
                case 13:    return 100000000000000ULL;
                case 14:    return 1000000000000000ULL;
                case 15:    return 10000000000000000ULL;
                case 16:    return 100000000000000000ULL;
                case 17:    return 1000000000000000000ULL;
                case 18:    return 10000000000000000000ULL;
                default:    return 0;
            }
        }

        /* INLINE uint64_t get_pow10_asm (uint8_t n) {
            const uint64_t jmp_dist = 15 * n; //n * 1;
            uint64_t result;
            asm volatile (
                "lea case_0(%%rip), %[result]\n" // we use the result register for storing the jump dest first
                "add %[jmp_dist], %[result]\n"
                "jmp *%[result]\n"

                "case_0:\n"
                "movabs $10,  %[result]\n"
                "jmp done\n"

                "case_1:\n"
                "movabs $100,  %[result]\n"
                "jmp done\n"

                "case_2:\n"
                "movabs $1000,  %[result]\n"
                "jmp done\n"

                "case_3:\n"
                "movabs $10000,  %[result]\n"
                "jmp done\n"

                "case_4:\n"
                "movabs $100000,  %[result]\n"
                "jmp done\n"

                "case_5:\n"
                "movabs $1000000,  %[result]\n"
                "jmp done\n"

                "case_6:\n"
                "movabs $10000000,  %[result]\n"
                "jmp done\n"

                "case_7:\n"
                "movabs $100000000,  %[result]\n"
                "jmp done\n"

                "case_8:\n"
                "movabs $1000000000,  %[result]\n"
                "jmp done\n"

                "case_9:\n"
                "movabs $10000000000,  %[result]\n"
                "jmp done\n"

                "case_10:\n"
                "movabs $100000000000,  %[result]\n"
                "jmp done\n"

                "case_11:\n"
                "movabs $1000000000000,  %[result]\n"
                "jmp done\n"

                "case_12:\n"
                "movabs $10000000000000,  %[result]\n"
                "jmp done\n"

                "case_13:\n"
                "movabs $100000000000000,  %[result]\n"
                "jmp done\n"

                "case_14:\n"
                "movabs $1000000000000000,  %[result]\n"
                "jmp done\n"

                "case_15:\n"
                "movabs $10000000000000000,  %[result]\n"
                "jmp done\n"

                "case_16:\n"
                "movabs $100000000000000000,  %[result]\n"
                "jmp done\n"

                "case_17:\n"
                "movabs $1000000000000000000,  %[result]\n"
                "jmp done\n"

                "case_18:\n"
                "movabs $10000000000000000000,  %[result]\n"
                "jmp done\n"

                "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
                "done:\n"

                : [result] "+r" (result)
                : [jmp_dist] "r" (jmp_dist)
                :
            );
            return result;
        } */

        /** */
        template <uint64or32_c T>
        INLINE constexpr uint32_t _log10 (T value) {
            uint32_t b;
            if constexpr (std::is_same_v<T, uint64_t>) {
                b = -(value > 0) & (63 - __builtin_clzll(value));
            } else {
                b = -(value > 0) & (31 - __builtin_clz(value));
            }
            uint32_t a = (b * 77) >> 8;
            return a + (value >= get_pow10_switch(a));
        }

        template <uint64or32_c T>
        INLINE constexpr uint32_t _log10_unsafe (T value) { 
            uint32_t b;
            if constexpr (std::is_same_v<T, uint64_t>) {
                b = (63 - __builtin_clzll(value));
            } else {
                b = (31 - __builtin_clz(value));
            }
            uint32_t a = (b * 77) >> 8;
            return a + (value >= get_pow10_switch(a));
        }

        #undef GET_POW10
    }

    namespace log10_impl_2 {
        INLINE constexpr uint8_t get_digits_table (uint32_t lz) {
            static constexpr char digits[64] = {
                0,
                0,
                0,
                1,
                1,
                1,
                2,
                2,
                2,
                3,
                3,
                3,
                3,
                4,
                4,
                4,
                5,
                5,
                5,
                6,
                6,
                6,
                6,
                7,
                7,
                7,
                8,
                8,
                8,
                9,
                9,
                9,
                9,
                10,
                10,
                10,
                11,
                11,
                11,
                12,
                12,
                12,
                12,
                13,
                13,
                13,
                14,
                14,
                14,
                15,
                15,
                15,
                15,
                16,
                16,
                16,
                17,
                17,
                17,
                18,
                18,
                18,
                18,
                19
            };
            return digits[lz];
        }

        INLINE constexpr uint8_t get_digits_switch (uint32_t lz) {
            switch (lz) {
                case 0:     return 0;
                case 1:     return 0;
                case 2:     return 0;
                case 3:     return 1;
                case 4:     return 1;
                case 5:     return 1;
                case 6:     return 2;
                case 7:     return 2;
                case 8:     return 2;
                case 9:     return 3;
                case 10:    return 3;
                case 11:    return 3;
                case 12:    return 3;
                case 13:    return 4;
                case 14:    return 4;
                case 15:    return 4;
                case 16:    return 5;
                case 17:    return 5;
                case 18:    return 5;
                case 19:    return 6;
                case 20:    return 6;
                case 21:    return 6;
                case 22:    return 6;
                case 23:    return 7;
                case 24:    return 7;
                case 25:    return 7;
                case 26:    return 8;
                case 27:    return 8;
                case 28:    return 8;
                case 29:    return 9;
                case 30:    return 9;
                case 31:    return 9;
                case 32:    return 9;
                case 33:    return 10;
                case 34:    return 10;
                case 35:    return 10;
                case 36:    return 11;
                case 37:    return 11;
                case 38:    return 11;
                case 39:    return 12;
                case 40:    return 12;
                case 41:    return 12;
                case 42:    return 12;
                case 43:    return 13;
                case 44:    return 13;
                case 45:    return 13;
                case 46:    return 14;
                case 47:    return 14;
                case 48:    return 14;
                case 49:    return 15;
                case 50:    return 15;
                case 51:    return 15;
                case 52:    return 15;
                case 53:    return 16;
                case 54:    return 16;
                case 55:    return 16;
                case 56:    return 17;
                case 57:    return 17;
                case 58:    return 17;
                case 59:    return 18;
                case 60:    return 18;
                case 61:    return 18;
                case 62:    return 18;
                case 63:    
                default:    return 0;
            }
        }

        INLINE constexpr uint64_t get_pow10_table (uint8_t n) {
            static constexpr uint64_t pow10[20] = {
                1ULL,
                10ULL,
                100ULL,
                1000ULL,   
                10000ULL,
                100000ULL,
                1000000ULL,
                10000000ULL,
                100000000ULL,
                1000000000ULL,
                10000000000ULL,
                100000000000ULL,
                1000000000000ULL,
                10000000000000ULL,
                100000000000000ULL,
                1000000000000000ULL,
                10000000000000000ULL,
                100000000000000000ULL,
                1000000000000000000ULL,
                10000000000000000000ULL
            };
            return pow10[n];
        }

        INLINE constexpr uint64_t get_pow10_switch (uint8_t n) {
            switch (n) {
                case 0:     return 1ULL;
                case 1:     return 10ULL;
                case 2:     return 100ULL;
                case 3:     return 1000ULL;
                case 4:     return 10000ULL;
                case 5:     return 100000ULL;
                case 6:     return 1000000ULL;
                case 7:     return 10000000ULL;
                case 8:     return 100000000ULL;
                case 9:     return 1000000000ULL;
                case 10:    return 10000000000ULL;
                case 11:    return 100000000000ULL;
                case 12:    return 1000000000000ULL;
                case 13:    return 10000000000000ULL;
                case 14:    return 100000000000000ULL;
                case 15:    return 1000000000000000ULL;
                case 16:    return 10000000000000000ULL;
                case 17:    return 100000000000000000ULL;
                case 18:    return 1000000000000000000ULL;
                case 19:    return 10000000000000000000ULL;
                default:    return 0;
            }
        }

        /* INLINE uint64_t get_pow10_asm (uint8_t n) {
            const uint64_t jmp_dist = 15 * n; //n * 1;
            uint64_t result;
            asm volatile (
                "lea case_0(%%rip), %[result]\n" // we use the result register for storing the jump dest first
                "add %[jmp_dist], %[result]\n"
                "jmp *%[result]\n"

                "case_0:\n"
                "movabs $1,  %[result]\n"
                "jmp done\n"

                "case_1:\n"
                "movabs $10,  %[result]\n"
                "jmp done\n"

                "case_2:\n"
                "movabs $100,  %[result]\n"
                "jmp done\n"

                "case_3:\n"
                "movabs $1000,  %[result]\n"
                "jmp done\n"

                "case_4:\n"
                "movabs $10000,  %[result]\n"
                "jmp done\n"

                "case_5:\n"
                "movabs $100000,  %[result]\n"
                "jmp done\n"

                "case_6:\n"
                "movabs $1000000,  %[result]\n"
                "jmp done\n"

                "case_7:\n"
                "movabs $10000000,  %[result]\n"
                "jmp done\n"

                "case_8:\n"
                "movabs $100000000,  %[result]\n"
                "jmp done\n"

                "case_9:\n"
                "movabs $1000000000,  %[result]\n"
                "jmp done\n"

                "case_10:\n"
                "movabs $10000000000,  %[result]\n"
                "jmp done\n"

                "case_11:\n"
                "movabs $100000000000,  %[result]\n"
                "jmp done\n"

                "case_12:\n"
                "movabs $1000000000000,  %[result]\n"
                "jmp done\n"

                "case_13:\n"
                "movabs $10000000000000,  %[result]\n"
                "jmp done\n"

                "case_14:\n"
                "movabs $100000000000000,  %[result]\n"
                "jmp done\n"

                "case_15:\n"
                "movabs $1000000000000000,  %[result]\n"
                "jmp done\n"

                "case_16:\n"
                "movabs $10000000000000000,  %[result]\n"
                "jmp done\n"

                "case_17:\n"
                "movabs $100000000000000000,  %[result]\n"
                "jmp done\n"

                "case_18:\n"
                "movabs $1000000000000000000,  %[result]\n"
                "jmp done\n"

                "case_19:\n"
                "movabs $10000000000000000000,  %[result]\n"
                "jmp done\n"

                "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
                "done:\n"

                : [result] "+r" (result)
                : [jmp_dist] "r" (jmp_dist)
                :
            );
            return result;
        } */

        INLINE constexpr uint32_t log10_unsafe (uint64_t v) {
            uint32_t lz = 63 ^ __builtin_clzll(v);
            
            uint32_t guess = get_digits_switch(lz);
        
            return guess - (v < get_pow10_switch(guess));
        }
    }

    using namespace log10_impl_1;
    INLINE constexpr uint32_t log10 (uint8_t value) {
        return _log10(static_cast<uint32_t>(value));
    }
    INLINE constexpr uint32_t log10 (uint16_t value) {
        return _log10(static_cast<uint32_t>(value));
    }
    INLINE constexpr uint32_t log10 (uint32_t value) {
        return _log10(value);
    }
    INLINE constexpr uint32_t log10 (uint64_t value) {
        return _log10(value);
    }

    INLINE constexpr uint32_t log10_unsafe (uint8_t value) {
        return _log10_unsafe(static_cast<uint32_t>(value));
    }
    INLINE constexpr uint32_t log10_unsafe (uint16_t value) {
        return _log10_unsafe(static_cast<uint32_t>(value));
    }
    INLINE constexpr uint32_t log10_unsafe (uint32_t value) {
        return _log10_unsafe(value);
    }
    INLINE constexpr uint32_t log10_unsafe (uint64_t value) {
        return _log10_unsafe(value);
    }

    INLINE constexpr uint64_t sum_of_digits_unsafe (uint64_t value) {
        uint32_t c = log10_unsafe(value);
        // 10ull
        // 190ull
        // 2890ull
        // 38890ull
        // 488890ull
        // 5888890ull
        // 68888890ull
        // 788888890ull
        // 8888888890ull
        // 98888888890ull
        // 1088888888890ull
        // 11888888888890ull
        // 128888888888890ull
        // 1388888888888890ull
        // 14888888888888890ull
        // 158888888888888890ull
        // 1688888888888888890ull
        // 17888888888888888890ull
        // 188888888888888888890ull

        // 10
        // 190
        // 2890
        // 38890
        // 488890
        // 5888890
        // 68888890
        // 788888890
        // 8888888890
        // 98888888890
        // 1088888888890
        // 11888888888890
        // 128888888888890
        // 1388888888888890
        // 14888888888888890
        // 158888888888888890
        // 1688888888888888890
        // 17888888888888888890
        // 188888888888888888890
        switch (c) {
            case 0: {
                return value;
            }
            case 1: {
                return 10ull + (value - 10ull) * 2;
            }
            case 2: {
                return 190ull + (value - 100ull) * 3;
            }
            case 3: {
                return 2890ull + (value - 1000ull) * 4;
            }
            case 4: {
                return 38890ull + (value - 10000ull) * 5;
            }
            case 5: {
                return 488890ull + (value - 100000ull) * 6;
            }
            case 6: {
                return 5888890ull + (value - 1000000ull) * 7;
            }
            case 7: {
                return 68888890ull + (value - 10000000ull) * 8;
            }
            case 8: {
                return 788888890ull + (value - 100000000ull) * 9;
            }
            case 9: {
                return 8888888890ull + (value - 1000000000ull) * 10;
            }
            case 10: {
                return 98888888890ull + (value - 10000000000ull) * 11;
            }
            case 11: {
                return 1088888888890ull + (value - 100000000000ull) * 12;
            }
            case 12: {
                return 11888888888890ull + (value - 1000000000000ull) * 13;
            }
            case 13: {
                return 128888888888890ull + (value - 10000000000000ull) * 14;
            }
            case 14: {
                return 1388888888888890ull + (value - 100000000000000ull) * 15;
            }
            case 15: {
                return 14888888888888890ull + (value - 1000000000000000ull) * 16;
            }
            case 16: {
                return 158888888888888890ull + (value - 10000000000000000ull) * 17;
            }
            case 17: {
                return 1688888888888888890ull + (value - 100000000000000000ull) * 18;
            }
            case 18: {
                return 17888888888888888890ull + (value - 1000000000000000000ull) * 19;
            }
            default: {
                write(STDOUT_FILENO, "Error in sum_of_digits_unsafe()", 31);
                exit(1);
            }
        }
    }

    auto a = sum_of_digits_unsafe(11);
}