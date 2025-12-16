#pragma once

#include <ranges>
#include "./dp_bitset_base.hpp"
#include "../variant_optimizer/data.hpp"

namespace sum_intersection_dp_bitset {

using dp_bitset_base::word_t, dp_bitset_base::num_t;

inline void generate_bits (
    word_t* const bits,
    const num_t bitset_words,
    const num_t target,
    const QueuedField* const queued_fields_buffer,
    const VariantLeafMeta& meta
) {

    dp_bitset_base::init_bits(bits, bitset_words);

    for (const QueuedField& field : std::ranges::subrange{
        queued_fields_buffer,
        queued_fields_buffer + meta.ends.align1
    }) {
        auto num = field.size;
        if (num == 0) continue;
        BSSERT(num <= target);
        dp_bitset_base::apply_num_unsafe(num, bits, bitset_words);
    }
}

} // namespace sum_intersection_dp_bitset