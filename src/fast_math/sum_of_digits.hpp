#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "./log.hpp"


namespace fast_math {

    /* Sums up excluding value */
    template <std::unsigned_integral T, std::unsigned_integral U = size_t>
    constexpr U sum_of_digits_unsafe (T value) {
        uint32_t c = log_unsafe<10>(value);

        // 10ULL
        // 190ULL
        // 2890ULL
        // 38890ULL
        // 488890ULL
        // 5888890ULL
        // 68888890ULL
        // 788888890ULL
        // 8888888890ULL
        // 98888888890ULL
        // 1088888888890ULL
        // 11888888888890ULL
        // 128888888888890ULL
        // 1388888888888890ULL
        // 14888888888888890ULL
        // 158888888888888890ULL
        // 1688888888888888890ULL
        // 17888888888888888890ULL
        // 188888888888888888890ULL

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
                if constexpr (sizeof(U) >= 1) {
                    return 10ULL + ((value - 10ULL) * 2);
                }
            }
            case 2: {
                if constexpr (sizeof(U) >= 2) {
                    return 190ULL + ((value - 100ULL) * 3);
                }
            }
            case 3: {
                if constexpr (sizeof(U) >= 2) {
                    return 2890ULL + ((value - 1000ULL) * 4);
                }
            }
            case 4: {
                if constexpr (sizeof(U) >= 3) {
                    return 38890ULL + ((value - 10000ULL) * 5);
                }
            }
            case 5: {
                if constexpr (sizeof(U) >= 3) {
                    return 488890ULL + ((value - 100000ULL) * 6);
                }
            }
            case 6: {
                if constexpr (sizeof(U) >= 4) {
                    return 5888890ULL + ((value - 1000000ULL) * 7);
                }
            }
            case 7: {
                if constexpr (sizeof(U) >= 4) {
                    return 68888890ULL + ((value - 10000000ULL) * 8);
                }
            }
            case 8: {
                if constexpr (sizeof(U) >= 5) {
                    return 788888890ULL + ((value - 100000000ULL) * 9);
                }
            }
            case 9: {
                if constexpr (sizeof(U) >= 5) {
                    return 8888888890ULL + ((value - 1000000000ULL) * 10);
                }
            }
            case 10: {
                if constexpr (sizeof(U) >= 5) {
                    return 98888888890ULL + ((value - 10000000000ULL) * 11);
                }
            }
            case 11: {
                if constexpr (sizeof(U) >= 6) {
                    return 1088888888890ULL + ((value - 100000000000ULL) * 12);
                }
            }
            case 12: {
                if constexpr (sizeof(U) >= 6) {
                    return 11888888888890ULL + ((value - 1000000000000ULL) * 13);
                }
            }
            case 13: {
                if constexpr (sizeof(U) >= 7) {
                    return 128888888888890ULL + ((value - 10000000000000ULL) * 14);
                }
            }
            case 14: {
                if constexpr (sizeof(U) >= 7) {
                    return 1388888888888890ULL + ((value - 100000000000000ULL) * 15);
                }
            }
            case 15: {
                if constexpr (sizeof(U) >= 7) {
                    return 14888888888888890ULL + ((value - 1000000000000000ULL) * 16);
                }
            }
            case 16: {
                if constexpr (sizeof(U) >= 8) {
                    return 158888888888888890ULL + ((value - 10000000000000000ULL) * 17);
                }
            }
            case 17: {
                if constexpr (sizeof(U) >= 8) {
                    return 1688888888888888890ULL + ((value - 100000000000000000ULL) * 18);
                }
            }
            case 18: {
                if constexpr (sizeof(U) >= 8) {
                    return 17888888888888888890ULL + ((value - 1000000000000000000ULL) * 19);
                }
            }
            case 19: {
                if constexpr (sizeof(U) >= 9) {
                    static constexpr __uint128_t base = (static_cast<__uint128_t>(1888888888888888888ULL) * 100) + 90;
                    return base + ((static_cast<__uint128_t>(value) - 10000000000000000000ULL) * 20);
                }
            }
            default: {
                std::unreachable();
            }
        }
    }
}