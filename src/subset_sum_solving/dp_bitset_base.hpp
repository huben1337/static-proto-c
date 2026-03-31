#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <gsl/pointers>
#include <type_traits>
#include <immintrin.h>
#include <xmmintrin.h>


namespace dp_bitset_base {

using num_t = uint64_t;

#if __AVX512DQ__

using word_t = __m512i;
using lane_t = uint64_t;

#define mmXXX_setzero_siXXX _mm512_setzero_si512
#define mmXXX_and_siXXX _mm512_and_si512
#define mmXXX_or_siXXX _mm512_or_si512
#define mmXXX_mask_set1_epi64 _mm512_mask_set1_epi64
#define mmXXX_movm_epi64 _mm512_movm_epi64

#elif __AVX2__

#include <utility>

using word_t = __m256i;
using lane_t = uint64_t;

#define mmXXX_setzero_siXXX _mm256_setzero_si256
#define mmXXX_and_siXXX _mm256_and_si256
#define mmXXX_or_siXXX _mm256_or_si256
#define mmXXX_mask_set1_epi64 _mm256_mask_set1_epi64
#define mmXXX_movm_epi64 _mm256_movm_epi64

#else

#error Unsupported CPU

#endif

using slane_t = std::make_signed_t<lane_t>;
constexpr uint32_t WORD_BYTES = sizeof(word_t);
constexpr uint32_t WORD_BITS = WORD_BYTES * 8;
constexpr uint32_t LANE_BYTES = sizeof(lane_t);
constexpr uint32_t LANE_BITS = LANE_BYTES * 8;
constexpr uint32_t WORD_LANE_COUNT = WORD_BYTES / LANE_BYTES;


[[nodiscard, gnu::always_inline]] constexpr num_t bitset_word_count (num_t target) {
    return (target + WORD_BITS) / WORD_BITS;
}

[[nodiscard]] constexpr bool bit_at (word_t* words, num_t target) {
    return (reinterpret_cast<uint8_t*>(words)[target / 8] & (uint8_t{1} << (target % 8))) != 0;
}

enum OnesStrategys : uint8_t {
    FULL_LUT,
    HYBRID_LUT_SCALAR,
    HYBRID_LUT_VECTOR,
    ARITHMETIC_SCALAR,
    ARITHMETIC_VECTOR
};

enum FillDirection : uint8_t {
    TO,
    FROM
};

namespace detail {
    template <FillDirection direction>
    [[nodiscard, gnu::always_inline]] constexpr slane_t make_partial_lane (const uint16_t n) {
        const uint8_t remaining = n % LANE_BITS;
        if constexpr (direction == TO) {
            return std::bit_cast<slane_t>((lane_t{1} << remaining) - 1);
        } else {
            return std::bit_cast<slane_t>(-(lane_t{1} << remaining));
        }
    }

    template <FillDirection direction>
    [[nodiscard, gnu::always_inline]] constexpr __mmask8 make_lane_value_mask (const __mmask8 full_lane_mask) {
        if constexpr (direction == TO) {
            return full_lane_mask - 1;
        } else {
            return 0 - full_lane_mask;
        }
    }
}


// 0 <= n < WORD_BITS
template <FillDirection direction, OnesStrategys strategy = HYBRID_LUT_VECTOR>
[[nodiscard, gnu::always_inline]] constexpr word_t ones (const uint16_t n) {
    if constexpr (strategy == FULL_LUT) {
        struct Table {
            word_t data[WORD_BITS];
            consteval Table () {
                for (size_t i = 0; i < WORD_BITS; i++) {
                    data[i] = ones<direction, HYBRID_LUT_VECTOR>(i);
                }
            }
        };
        constexpr Table table;

        return table.data[n];
    } else {
        const uint16_t lane_idx = n / LANE_BITS;

        if constexpr (strategy == HYBRID_LUT_SCALAR || strategy == HYBRID_LUT_VECTOR) {
            struct Table {
                word_t data[WORD_LANE_COUNT];
                consteval Table () {
                    for (size_t i = 0; i < WORD_LANE_COUNT; i++) {
                        word_t word {};
                        for (
                            size_t j = direction == TO ? 0 : i;
                            j < (direction == TO ? i : WORD_LANE_COUNT);
                            j++
                        ) {
                            word[j] = -1;
                        }
                        data[i] = word;
                    }
                }
            };
            constexpr Table full_lane_words;

            #define MAKE_WORD() full_lane_words.data[lane_idx]

            if constexpr (strategy == HYBRID_LUT_SCALAR) {
                return mmXXX_mask_set1_epi64(
                    MAKE_WORD(),
                    __mmask8{1} << lane_idx,
                    detail::make_partial_lane<direction>(n)
                );
            } else {
                word_t word = MAKE_WORD();
                word[lane_idx] = detail::make_partial_lane<direction>(n);
                return word;
            }

            #undef MAKE_WORD
        } else {
            const uint32_t full_words_mask = 1 << lane_idx;

            #define MAKE_WORD() mmXXX_movm_epi64(detail::make_lane_value_mask<direction>(full_words_mask))

            if constexpr (strategy == ARITHMETIC_SCALAR) {
                return mmXXX_mask_set1_epi64(
                    MAKE_WORD(),
                    full_words_mask,
                    detail::make_partial_lane<direction>(n)
                );
            } else {
                word_t word = MAKE_WORD();
                word[lane_idx] = detail::make_partial_lane<direction>(n);
                return word;
            }

            #undef MAKE_WORD
        }
    }
}

inline void and_merge (word_t* const bigger_bits, word_t* const smaller_bits, const num_t smaller_bits_count, const num_t word_offset = 0) {   
    const num_t full_bitset_words = smaller_bits_count / WORD_BITS;
    for (num_t i = 0; i < full_bitset_words; i++) {
        const num_t j = i + word_offset;
        bigger_bits[j] = mmXXX_and_siXXX(bigger_bits[j], smaller_bits[i]);
    }

    const uint16_t left_over_bits = smaller_bits_count % WORD_BITS;
    
    if (left_over_bits != 0) {
        const num_t& i = full_bitset_words;
        const num_t j = i + word_offset;

        bigger_bits[j] = mmXXX_and_siXXX(
            bigger_bits[j],
            mmXXX_or_siXXX(smaller_bits[i], ones<FROM>(left_over_bits))
        );
    }
}

[[gnu::always_inline]] inline void init_bits (word_t* const words, const num_t word_count) {
    #if __AVX512DQ__
        words[0] = _mm512_set_epi64(0, 0, 0, 0, 0, 0, 0, 1);
    #elif __AVX2__
        words[0] = _mm256_set_epi64x(0, 0, 0, 1);
    #else
        #error
    #endif
    for (word_t* p = words + 1; p < words + word_count; p++) {
        p[0] = mmXXX_setzero_siXXX();
    }
}


#if __AVX512DQ__

constexpr __m512i mm512_lsl_epi64_idxs[9] {
    {0|0, 1|0, 2|0, 3|0, 4|0, 5|0, 6|0, 7|0},
    {7|8, 0|0, 1|0, 2|0, 3|0, 4|0, 5|0, 6|0},
    {6|8, 7|8, 0|0, 1|0, 2|0, 3|0, 4|0, 5|0},
    {5|8, 6|8, 7|8, 0|0, 1|0, 2|0, 3|0, 4|0},
    {4|8, 5|8, 6|8, 7|8, 0|0, 1|0, 2|0, 3|0},
    {3|8, 4|8, 5|8, 6|8, 7|8, 0|0, 1|0, 2|0},
    {2|8, 3|8, 4|8, 5|8, 6|8, 7|8, 0|0, 1|0},
    {1|8, 2|8, 3|8, 4|8, 5|8, 6|8, 7|8, 0|0},
    {0|8, 1|8, 2|8, 3|8, 4|8, 5|8, 6|8, 7|8}
};

#define mm512_lsl_epi64_(A, B, N) _mm512_permutex2var_epi64(A, mm512_lsl_epi64_idxs[N], B)

[[gnu::always_inline]] inline void apply_num_unsafe (const num_t num, word_t* const words, num_t word_count) {
    const uint8_t bit_shift = num % LANE_BITS;
    const uint8_t rbit_shift = LANE_BITS - bit_shift;
    const uint8_t lane_shift = (num / LANE_BITS) % WORD_LANE_COUNT;
    const num_t word_shift = num / WORD_BITS;

    const num_t last_out_idx = word_count - 1;
    const num_t last_in_idx = last_out_idx - word_shift;

    word_t last_in = words[last_in_idx];
    word_t next_ovflow = _mm512_srli_epi64(last_in, rbit_shift);
    word_t next_lane_bit_shifted = _mm512_slli_epi64(last_in, bit_shift);

    if (last_in_idx >= 1) {
        for (num_t out_word_idx = last_out_idx; ; out_word_idx--) {
            const num_t prev_idx = out_word_idx - word_shift - 1;

            word_t prev = words[prev_idx];

            word_t curr_ovflw = next_ovflow;
            word_t prev_ovflw = _mm512_srli_epi64(prev, rbit_shift);
            next_ovflow = prev_ovflw;
            
            word_t curr_lane_bit_shifted = next_lane_bit_shifted;
            word_t curr_bit_shifted = _mm512_or_si512(
                curr_lane_bit_shifted,
                _mm512_alignr_epi64(curr_ovflw, prev_ovflw, WORD_LANE_COUNT - 1)
            );

            word_t prev_lane_bit_shifted = _mm512_slli_epi64(prev, bit_shift);
            next_lane_bit_shifted = prev_lane_bit_shifted;
            word_t prev_bit_shifted = _mm512_or_si512(
                prev_lane_bit_shifted,
                _mm512_alignr_epi64(prev_ovflw, _mm512_setzero_si512(), WORD_LANE_COUNT - 1)
            );

            word_t* out_word_ptr = words + out_word_idx;
            out_word_ptr[0] = _mm512_or_si512(
                out_word_ptr[0],
                mm512_lsl_epi64_(curr_bit_shifted, prev_bit_shifted, lane_shift)
            );

            if (prev_idx == 0) break;
        }
    }
    {
        word_t curr_ovflw = next_ovflow;
        word_t curr_lane_bit_shifted = next_lane_bit_shifted;
        word_t curr_bit_shifted = _mm512_or_si512(
            curr_lane_bit_shifted,
            _mm512_alignr_epi64(curr_ovflw, _mm512_setzero_si512(), WORD_LANE_COUNT - 1)
        );

        word_t* out_word_ptr = words + word_shift;
        out_word_ptr[0] = _mm512_or_si512(
            out_word_ptr[0],
            mm512_lsl_epi64_(curr_bit_shifted, _mm512_setzero_si512(), lane_shift)
        );
    }
}

#undef mm512_lsl_epi64_

#elif __AVX2__

template <size_t N>
[[nodiscard, clang::always_inline]] static inline __m256i mm256_lsl_epi64_ (const __m256i a, const __m256i b) {
    if constexpr (N == 0) {
        return a;
    } else if constexpr (N == 1) {
        // [a2, a1, a0, b3]
        __m256i a_perm = _mm256_permute4x64_epi64(a, _MM_SHUFFLE(2,1,0,0));
        __m256i b_perm = _mm256_permute4x64_epi64(b, _MM_SHUFFLE(3,3,3,3));
        return _mm256_blend_epi32(a_perm, b_perm, 0b00000011);  // lowest 64 bits from b
    } else if constexpr (N == 2) {
        // [a1, a0, b3, b2]
        //__m256i a_perm = _mm256_permute4x64_epi64(a, _MM_SHUFFLE(1,0,0,0));
        //__m256i b_perm = _mm256_permute4x64_epi64(b, _MM_SHUFFLE(3,3,3,2));
        //return _mm256_blend_epi32(a_perm, b_perm, 0b00001111);  // lowest 128 bits from b
        return _mm256_permute2x128_si256(a, b, 3);
    } else if constexpr (N == 3) {
        // [a0, b3, b2, b1]
        __m256i a_perm = _mm256_permute4x64_epi64(a, _MM_SHUFFLE(0,0,0,0));
        __m256i b_perm = _mm256_permute4x64_epi64(b, _MM_SHUFFLE(3,3,2,1));
        return _mm256_blend_epi32(a_perm, b_perm, 0b00111111);  // lowest 192 bits from b
    } else if constexpr (N == 4) {
        return b;
    } else {
        static_assert(false, "shouldn't shift more than 4 lanes");
    }
}

namespace {

template <size_t lane_shift>
[[clang::always_inline]] inline void apply_num_unsafe_ (const num_t num, word_t* const words, const num_t word_count) {
    const uint8_t bit_shift = num % LANE_BITS;
    const uint8_t rbit_shift = LANE_BITS - bit_shift;
    const num_t word_shift = num / WORD_BITS;

    const num_t last_out_idx = word_count - 1;
    const num_t last_in_idx = last_out_idx - word_shift;

    word_t last_in = words[last_in_idx];
    word_t next_ovflow = _mm256_srli_epi64(last_in, rbit_shift);
    word_t next_lane_bit_shifted = _mm256_slli_epi64(last_in, bit_shift);

    if (last_in_idx >= 1) {
        for (num_t out_word_idx = last_out_idx; ; out_word_idx--) {
            const num_t prev_idx = out_word_idx - word_shift - 1;

            word_t prev = words[prev_idx];

            word_t curr_ovflw = next_ovflow;
            word_t prev_ovflw = _mm256_srli_epi64(prev, rbit_shift);
            next_ovflow = prev_ovflw;
            
            word_t curr_lane_bit_shifted = next_lane_bit_shifted;
            word_t curr_bit_shifted = _mm256_or_si256(
                curr_lane_bit_shifted,
                mm256_lsl_epi64_<1>(curr_ovflw, prev_ovflw)
            );

            word_t prev_lane_bit_shifted = _mm256_slli_epi64(prev, bit_shift);
            next_lane_bit_shifted = prev_lane_bit_shifted;
            word_t prev_bit_shifted = _mm256_or_si256(
                prev_lane_bit_shifted,
                mm256_lsl_epi64_<1>(prev_ovflw, _mm256_setzero_si256())
            );

            word_t* out_word_ptr = words + out_word_idx;
            out_word_ptr[0] = _mm256_or_si256(
                out_word_ptr[0],
                mm256_lsl_epi64_<lane_shift>(curr_bit_shifted, prev_bit_shifted)
            );

            if (prev_idx == 0) break;
        }
    }
    {
        word_t curr_ovflw = next_ovflow;
        word_t curr_lane_bit_shifted = next_lane_bit_shifted;
        word_t curr_bit_shifted = _mm256_or_si256(
            curr_lane_bit_shifted,
            mm256_lsl_epi64_<1>(curr_ovflw, _mm256_setzero_si256())
        );

        word_t* out_word_ptr = words + word_shift;
        out_word_ptr[0] = _mm256_or_si256(
            out_word_ptr[0],
            mm256_lsl_epi64_<lane_shift>(curr_bit_shifted, _mm256_setzero_si256())
        );
    }
}

} // namespace

[[gnu::always_inline]] inline void apply_num_unsafe (const num_t num, word_t* const words, num_t word_count) {
    const uint16_t lane_shift = (num / LANE_BITS) % WORD_LANE_COUNT;

    switch (lane_shift) {
        case 0:
            apply_num_unsafe_<0>(num, words, word_count);
            break;
        case 1:
            apply_num_unsafe_<1>(num, words, word_count);
            break;
        case 2:
            apply_num_unsafe_<2>(num, words, word_count);
            break;
        case 3:
            apply_num_unsafe_<3>(num, words, word_count);
            break;
        default:
            std::unreachable();
    }
}

#endif

} // namespace dp_bitset_base