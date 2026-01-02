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

#elif __AVX2__

#include <utility>

using word_t = __m256i;
using lane_t = uint64_t;

#else

#error Unsupported CPU

#endif

using slane_t = std::make_signed_t<lane_t>;
constexpr size_t WORD_BYTES = sizeof(word_t);
constexpr size_t WORD_BITS = WORD_BYTES * 8;
constexpr size_t LANE_BYTES = sizeof(lane_t);
constexpr size_t LANE_BITS = LANE_BYTES * 8;
constexpr size_t WORD_LANE_COUNT = WORD_BYTES / LANE_BYTES;


[[gnu::always_inline]] constexpr num_t bitset_word_count (num_t target) {
    return (target + WORD_BITS) / WORD_BITS;
}

constexpr bool bit_at (word_t* words, num_t target) {
    return (reinterpret_cast<uint8_t*>(words)[target / 8] & (uint8_t{1} << (target % 8))) != 0;
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

constexpr __m512i shift_one_idx = mm512_lsl_epi64_idxs[1];

#define mm512_lsl_epi64_(A, B, N) _mm512_permutex2var_epi64(A, mm512_lsl_epi64_idxs[N], B)

[[gnu::always_inline]] inline void init_bits (word_t* const words, const num_t word_count) {
    words[0] = _mm512_set_epi64(0, 0, 0, 0, 0, 0, 0, 1);
    for (word_t* p = words + 1; p < words + word_count; p++) {
        p[0] = _mm512_setzero_si512();
    }
}

[[gnu::always_inline]] inline void apply_num_unsafe (const num_t num, word_t* const words, num_t word_count) {
    const uint8_t bit_shift = num % LANE_BITS;
    const uint8_t rbit_shift = LANE_BITS - bit_shift;
    const uint8_t lane_shift = (num / LANE_BITS) % WORD_LANE_COUNT;
    const num_t word_shift = num / WORD_BITS;
    
    if (word_shift == 0) {
        word_t prev = _mm512_setzero_si512();
        word_t prev_ovflw = _mm512_setzero_si512();

        for (num_t i = 0; i < word_count; i++) {
            word_t* curr_ptr = words + i;
            word_t curr = curr_ptr[0];

            word_t ovflw = _mm512_srli_epi64(curr, rbit_shift);
            
            word_t curr_bit_shifted = _mm512_or_si512(
                _mm512_slli_epi64(curr, bit_shift),
                _mm512_permutex2var_epi64(ovflw, shift_one_idx, prev_ovflw)
            );
            prev_ovflw = ovflw;

            curr_ptr[0] = _mm512_or_si512(
                curr,
                mm512_lsl_epi64_(curr_bit_shifted, prev, lane_shift)
            );

            prev = curr_bit_shifted;
        }
    } else {
        word_t prev = _mm512_setzero_si512();
        word_t prev_ovflw = _mm512_setzero_si512();
        word_t buffer[word_shift];

        #pragma clang loop unroll(disable)
        for (num_t i = 0; i < word_shift; i++) {
            word_t curr = words[i];

            word_t ovflw = _mm512_srli_epi64(curr, rbit_shift);
            word_t curr_bit_shifted = _mm512_or_si512(
                _mm512_slli_epi64(curr, bit_shift),
                _mm512_permutex2var_epi64(ovflw, shift_one_idx, prev_ovflw)
            );
            prev_ovflw = ovflw;

            buffer[i] = _mm512_or_si512(
                curr,
                mm512_lsl_epi64_(curr_bit_shifted, prev, lane_shift)
            );

            prev = curr_bit_shifted;
        }

        num_t i = word_shift;
        for (; i < word_count - word_shift + 1; i += word_shift) {
            #pragma clang loop unroll(disable)
            for (num_t j = 0; j < word_shift; j++) {
                word_t* curr_ptr = words + j + i;
                word_t curr = curr_ptr[0];

                curr_ptr[0] = _mm512_or_si512(
                    curr,
                    buffer[j]
                );

                word_t ovflw = _mm512_srli_epi64(curr, rbit_shift);
                word_t curr_bit_shifted = _mm512_or_si512(
                    _mm512_slli_epi64(curr, bit_shift),
                    _mm512_permutex2var_epi64(ovflw, shift_one_idx, prev_ovflw)
                );

                buffer[j] = _mm512_or_si512(
                    curr,
                    mm512_lsl_epi64_(curr_bit_shifted, prev, lane_shift)
                );

                prev = curr_bit_shifted;
            }
        }
        
        #pragma clang loop unroll(disable)
        for (num_t j = 0; j < word_count - i; j++) {
            word_t* curr_ptr = words + j + i;
            word_t curr = curr_ptr[0];

            curr_ptr[0] = _mm512_or_si512(
                curr,
                buffer[j]
            );
        }
    }
}

// 0 <= n < 512
[[gnu::always_inline]] constexpr __m512i ones_up_to_n (const uint16_t n) {
    const size_t full_words = n / LANE_BITS;
    const size_t remaining = n % LANE_BITS;

    constexpr __m512i mm512_full_lane_parts[8] {
        { 0,  0,  0,  0,  0,  0,  0,  0},
        {-1,  0,  0,  0,  0,  0,  0,  0},
        {-1, -1,  0,  0,  0,  0,  0,  0},
        {-1, -1, -1,  0,  0,  0,  0,  0},
        {-1, -1, -1, -1,  0,  0,  0,  0},
        {-1, -1, -1, -1, -1,  0,  0,  0},
        {-1, -1, -1, -1, -1, -1,  0,  0},
        {-1, -1, -1, -1, -1, -1, -1,  0}
    };

    __m512i word = mm512_full_lane_parts[full_words];
    word[full_words] = std::bit_cast<slane_t>((lane_t{1} << remaining) - 1);
    return word;
}

// 0 <= n < 512
[[gnu::always_inline]] constexpr __m512i ones_up_from_n (const uint16_t n) {
    const size_t full_words = n / LANE_BITS;
    const size_t remaining = n % LANE_BITS;

    constexpr __m512i mm512_full_lane_parts[8] {
        {-1, -1, -1, -1, -1, -1, -1, -1},
        { 0, -1, -1, -1, -1, -1, -1, -1},
        { 0,  0, -1, -1, -1, -1, -1, -1},
        { 0,  0,  0, -1, -1, -1, -1, -1},
        { 0,  0,  0,  0, -1, -1, -1, -1},
        { 0,  0,  0,  0,  0, -1, -1, -1},
        { 0,  0,  0,  0,  0,  0, -1, -1},
        { 0,  0,  0,  0,  0,  0,  0, -1}
    };

    __m512i word = mm512_full_lane_parts[full_words];
    word[full_words] = std::bit_cast<slane_t>(-(lane_t{1} << remaining));
    return word;
}

inline void and_merge (word_t* const bigger_bits, word_t* const smaller_bits, const num_t smaller_words_count) {

    const num_t full_bitset_words = (smaller_words_count + 1) / WORD_BITS;
    for (num_t i = 0; i < full_bitset_words; i++) {
        bigger_bits[i] = _mm512_and_si512(bigger_bits[i], smaller_bits[i]);
    }

    const uint16_t left_over_bits = smaller_words_count - (full_bitset_words * WORD_BITS);

    if (left_over_bits != 0) {
        const num_t& i = full_bitset_words;

        bigger_bits[i] = _mm512_and_si512(
            bigger_bits[i],
            _mm512_or_si512(smaller_bits[i], ones_up_from_n(left_over_bits))
        );
    }
}

#undef mm512_lsl_epi64_

#elif __AVX2__


template <size_t N>
[[clang::always_inline]] static inline __m256i mm256_lsl_epi64_ (const __m256i a, const __m256i b) {
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
[[clang::always_inline]] inline void apply_num_unsafe_ (const uint64_t num, word_t* const words, const size_t word_count) {
    const uint16_t bit_shift = num % LANE_BITS;
    const uint16_t rbit_shift = LANE_BITS - bit_shift;
    const size_t word_shift = num / WORD_BITS;
    
    if (word_shift == 0) {
        word_t prev = _mm256_setzero_si256();
        word_t prev_ovflw = _mm256_setzero_si256();

        for (uint64_t i = 0; i < word_count; i++) {
            word_t* curr_ptr = words + i;
            word_t curr = _mm256_load_si256(curr_ptr);

            word_t ovflw = _mm256_srli_epi64(curr, rbit_shift);
            word_t curr_bit_shifted = _mm256_or_si256(
                _mm256_slli_epi64(curr, bit_shift),
                _mm256_blend_epi32(_mm256_permute4x64_epi64(ovflw, _MM_SHUFFLE(2,1,0,0)), _mm256_permute4x64_epi64(prev_ovflw, _MM_SHUFFLE(3,2,1,3)), 0b00000011)
            );
            prev_ovflw = ovflw;

            _mm256_store_si256(curr_ptr,
                _mm256_or_si256(
                    curr,
                    mm256_lsl_epi64_<lane_shift>(curr_bit_shifted, prev)
                )
            );

            prev = curr_bit_shifted;
        }
    } else {
        word_t prev = _mm256_setzero_si256();
        word_t prev_ovflw = _mm256_setzero_si256();
        word_t buffer[word_shift];

        #pragma clang loop unroll(disable)
        for (uint64_t i = 0; i < word_shift; i++) {
            word_t curr = _mm256_load_si256(words + i);

            word_t ovflw = _mm256_srli_epi64(curr, rbit_shift);
            word_t curr_bit_shifted = _mm256_or_si256(
                _mm256_slli_epi64(curr, bit_shift),
                _mm256_blend_epi32(_mm256_permute4x64_epi64(ovflw, _MM_SHUFFLE(2,1,0,0)), _mm256_permute4x64_epi64(prev_ovflw, _MM_SHUFFLE(3,2,1,3)), 0b00000011)
            );
            prev_ovflw = ovflw;

            _mm256_store_si256(buffer + i,
                _mm256_or_si256(
                    curr,
                    mm256_lsl_epi64_<lane_shift>(curr_bit_shifted, prev)
                )
            );

            prev = curr_bit_shifted;
        }


        uint64_t i = word_shift;
        for (; i < word_count - word_shift + 1; i += word_shift) {
            #pragma clang loop unroll(disable)
            for (uint64_t j = 0; j < word_shift; j++) {
                word_t* curr_ptr = words + j + i;
                word_t curr = _mm256_load_si256(curr_ptr);

                _mm256_store_si256(curr_ptr,
                    _mm256_or_si256(
                        curr,
                        _mm256_load_si256(buffer + j)
                    )
                );

                word_t ovflw = _mm256_srli_epi64(curr, rbit_shift);
                word_t curr_bit_shifted = _mm256_or_si256(
                    _mm256_slli_epi64(curr, bit_shift),
                    _mm256_blend_epi32(_mm256_permute4x64_epi64(ovflw, _MM_SHUFFLE(2,1,0,0)), _mm256_permute4x64_epi64(prev_ovflw, _MM_SHUFFLE(3,2,1,3)), 0b00000011)
                );

                _mm256_store_si256(buffer + j,
                    _mm256_or_si256(
                        curr,
                        mm256_lsl_epi64_<lane_shift>(curr_bit_shifted, prev)
                    )
                );

                prev = curr_bit_shifted;
            }
        }
        
        #pragma clang loop unroll(disable)
        for (uint64_t j = 0; j < word_count - i; j++) {
            word_t* curr_ptr = words + j + i;
            word_t curr = _mm256_load_si256(curr_ptr);

            _mm256_store_si256(curr_ptr,
                _mm256_or_si256(
                    curr,
                    _mm256_load_si256(buffer + j)
                )
            );
        }
    }
}

} // namespace

[[gnu::always_inline]] inline void init_bits (word_t* const words, const num_t word_count) {
    _mm256_store_si256(words, _mm256_set_epi64x(0, 0, 0, 1));
    for (word_t* p = words + 1; p < words + word_count; p++) {
        _mm256_store_si256(p, _mm256_setzero_si256());
    }
}

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

// 0 <= n < 256
[[gnu::always_inline]] constexpr __m256i ones_up_to_n (const uint16_t n) {
    const size_t full_words = n / LANE_BITS;
    const size_t remaining = n % LANE_BITS;

    constexpr __m256i mm256_full_lane_parts[4] {
        { 0,  0,  0,  0},
        {-1,  0,  0,  0},
        {-1, -1,  0,  0},
        {-1, -1, -1,  0}
    };

    __m256i word = mm256_full_lane_parts[full_words];
    word[full_words] = std::bit_cast<slane_t>((lane_t{1} << remaining) - 1);
    return word;
}

// 0 <= n < 256
[[gnu::always_inline]] constexpr __m256i ones_up_from_n (const uint16_t n) {
    const size_t full_words = n / LANE_BITS;
    const size_t remaining = n % LANE_BITS;

    constexpr __m256i mm256_full_lane_parts[4] {
        {-1, -1, -1, -1},
        { 0, -1, -1, -1},
        { 0,  0, -1, -1},
        { 0,  0,  0, -1}
    };

    __m256i word = mm256_full_lane_parts[full_words];
    word[full_words] = std::bit_cast<slane_t>(-(lane_t{1} << remaining));
    return word;
}

inline void and_merge (word_t* const bigger_bits, word_t* const smaller_bits, const num_t smaller_words_count) {

    const num_t full_bitset_words = (smaller_words_count + 1) / WORD_BITS;
    for (num_t i = 0; i < full_bitset_words; i++) {
        bigger_bits[i] = _mm256_and_si256(bigger_bits[i], smaller_bits[i]);
    }

    const uint16_t left_over_bits = smaller_words_count - (full_bitset_words * WORD_BITS);

    if (left_over_bits != 0) {
        const num_t& i = full_bitset_words;

        bigger_bits[i] = _mm256_and_si256(
            bigger_bits[i],
            _mm256_or_si256(smaller_bits[i], ones_up_from_n(left_over_bits))
        );
    }
}


#endif

} // namespace dp_bitset_base