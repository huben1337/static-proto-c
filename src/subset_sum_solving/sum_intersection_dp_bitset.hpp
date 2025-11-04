#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <gsl/pointers>
#include <type_traits>
#include <immintrin.h>
#include <xmmintrin.h>

#include "../util/logger.hpp"
#include "../variant_optimizer/data.hpp"

namespace sum_intersection_dp_bitset {

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

struct bitset {
    gsl::owner<word_t*> words;
    num_t size;
};

[[gnu::always_inline]] constexpr num_t bitset_words_count (num_t target) {
    return (target + WORD_BITS) / WORD_BITS;
}

[[gnu::always_inline]] constexpr gsl::owner<word_t*> allocate_bitset_words (num_t bitset_words) {
    gsl::owner<word_t*> words = static_cast<gsl::owner<word_t*>>(std::aligned_alloc(alignof(word_t), bitset_words * WORD_BYTES));
    BSSERT(words != nullptr, "Memory allocation failed.")
    return words;
}

void generate_bits (word_t* bits, num_t bitset_words, num_t target, const FixedLeaf* nums, uint16_t num_count);

inline bitset create_unsafe (const num_t target, const FixedLeaf* const nums, const uint16_t num_count) {
    const num_t bitset_words = bitset_words_count(target);

    gsl::owner<word_t*> bits = allocate_bitset_words(target);

    generate_bits(bits, bitset_words, target, nums, num_count);

    return {bits, target};
}

inline void merge (word_t* bigger_words, bitset smaller);

inline void apply_smaller_unsafe (word_t* const current_bits, const num_t target, const FixedLeaf* const nums, const uint16_t num_count) {
    const num_t bitset_words = bitset_words_count(target);

    word_t bits[bitset_words];
    
    generate_bits(bits, bitset_words, target, nums, num_count);
    
    merge(current_bits, {bits, target});
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

inline void generate_bits (word_t* const bits, const num_t bitset_words, const num_t target, const FixedLeaf* const nums, const uint16_t num_count) {

    bits[0] = _mm512_set_epi64(0, 0, 0, 0, 0, 0, 0, 1);
    for (word_t* p = bits + 1; p < bits + bitset_words; p++) {
        p[0] = _mm512_setzero_si512();
    }

    //#pragma clang loop vectorize(enable)
    const auto* const nums_end = nums + num_count;
    for (const auto *it = nums; it != nums_end; it++) {
        auto num = it->get_size();
        logger::debug("[generate_bits] num: ", num);
        if (num == 0 || num > target) continue;
        const uint8_t bit_shift = num % LANE_BITS;
        const uint8_t rbit_shift = LANE_BITS - bit_shift;
        const uint8_t lane_shift = (num / LANE_BITS) % WORD_LANE_COUNT;
        const num_t word_shift = num / WORD_BITS;
        
        if (word_shift == 0) {
            word_t prev = _mm512_setzero_si512();
            word_t prev_ovflw = _mm512_setzero_si512();

            for (num_t i = 0; i < bitset_words; i++) {
                word_t* curr_ptr = bits + i;
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
                word_t curr = bits[i];

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
            for (; i < bitset_words - word_shift + 1; i += word_shift) {
                #pragma clang loop unroll(disable)
                for (num_t j = 0; j < word_shift; j++) {
                    word_t* curr_ptr = bits + j + i;
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
            for (num_t j = 0; j < bitset_words - i; j++) {
                word_t* curr_ptr = bits + j + i;
                word_t curr = curr_ptr[0];

                curr_ptr[0] = _mm512_or_si512(
                    curr,
                    buffer[j]
                );
            }
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
    word[full_words] = static_cast<slane_t>((lane_t{1} << remaining) - 1);
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
    word[full_words] = static_cast<slane_t>(-(lane_t{1} << remaining));
    return word;
}

inline void merge (word_t* const bigger_words, bitset const smaller) {

    const num_t full_bitset_words = (smaller.size + 1) / WORD_BITS;
    for (num_t i = 0; i < full_bitset_words; i++) {
        bigger_words[i] = _mm512_and_si512(bigger_words[i], smaller.words[i]);
    }

    const uint16_t left_over_bits = smaller.size - (full_bitset_words * WORD_BITS);

    if (left_over_bits != 0) {
        const num_t& i = full_bitset_words;

        bigger_words[i] = _mm512_and_si512(
            bigger_words[i],
            _mm512_or_si512(smaller.words[i], ones_up_from_n(left_over_bits))
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

template <size_t lane_shift>
[[clang::always_inline]] static inline void subset_sum_dp_bitset_add_num (const uint64_t num, const size_t bitset_words, word_t* const bits) {
    const uint16_t bit_shift = num % LANE_BITS;
    const uint16_t rbit_shift = LANE_BITS - bit_shift;
    const size_t word_shift = num / WORD_BITS;
    
    if (word_shift == 0) {
        word_t prev = _mm256_setzero_si256();
        word_t prev_ovflw = _mm256_setzero_si256();

        for (uint64_t i = 0; i < bitset_words; i++) {
            word_t* curr_ptr = bits + i;
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
            word_t curr = _mm256_load_si256(bits + i);

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
        for (; i < bitset_words - word_shift + 1; i += word_shift) {
            #pragma clang loop unroll(disable)
            for (uint64_t j = 0; j < word_shift; j++) {
                word_t* curr_ptr = bits + j + i;
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
        for (uint64_t j = 0; j < bitset_words - i; j++) {
            word_t* curr_ptr = bits + j + i;
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


// O(n * t) | 0 < t < MAX_SUM
inline void generate_bits (word_t* const bits, const num_t bitset_words, const num_t target, const FixedLeaf* const nums, const uint16_t num_count) {

    _mm256_store_si256(bits, _mm256_set_epi64x(0, 0, 0, 1));
    for (word_t* p = bits + 1; p < bits + bitset_words; p++) {
        _mm256_store_si256(p, _mm256_setzero_si256());
    }

    const uint8_t& dp_target = reinterpret_cast<uint8_t*>(bits)[target / 8];
    const uint8_t dp_target_mask = 1U << (target % 8);

    //#pragma clang loop vectorize(enable)
    const auto* const nums_end = nums + num_count;
    for (const auto *it = nums; it != nums_end; it++) {
        auto num = it->get_size();
        logger::debug("[generate_bits] num: ", num);
        if (num == 0 || num > target) continue;
        const uint16_t lane_shift = (num / LANE_BITS) % WORD_LANE_COUNT;
        
        switch (lane_shift) {
            case 0:
                subset_sum_dp_bitset_add_num<0>(num, bitset_words, bits);
                break;
            case 1:
                subset_sum_dp_bitset_add_num<1>(num, bitset_words, bits);
                break;
            case 2:
                subset_sum_dp_bitset_add_num<2>(num, bitset_words, bits);
                break;
            case 3:
                subset_sum_dp_bitset_add_num<3>(num, bitset_words, bits);
                break;
            default:
                std::unreachable();
        }
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
    word[full_words] = static_cast<slane_t>((lane_t{1} << remaining) - 1);
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
    word[full_words] = static_cast<slane_t>(-(lane_t{1} << remaining));
    return word;
}

inline void merge (word_t* const bigger_words, bitset const smaller) {

    const num_t full_bitset_words = (smaller.size + 1) / WORD_BITS;
    for (num_t i = 0; i < full_bitset_words; i++) {
        bigger_words[i] = _mm256_and_si256(bigger_words[i], smaller.words[i]);
    }

    const uint16_t left_over_bits = smaller.size - (full_bitset_words * WORD_BITS);

    if (left_over_bits != 0) {
        const num_t& i = full_bitset_words;

        bigger_words[i] = _mm256_and_si256(
            bigger_words[i],
            _mm256_or_si256(smaller.words[i], ones_up_from_n(left_over_bits))
        );
    }
}


#endif

} // namespace sum_intersection_dp_bitset