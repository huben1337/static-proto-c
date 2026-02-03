#pragma once

#include "../../subset_sum_solving/dp_bitset_base.hpp"
#include "../layout_data.hpp"

namespace sum_intersection_dp_bitset {

using dp_bitset_base::word_t, dp_bitset_base::num_t;

inline void generate_bits (
    word_t* const bits,
    const num_t bitset_words,
    const num_t target,
    const std::span<const QueuedField> queued_fields_buffer,
    const VariantLeafMeta& meta
) {

    dp_bitset_base::init_bits(bits, bitset_words);

    for (const QueuedField& field : meta.field_idxs.access_subrange(queued_fields_buffer)) {
        auto num = field.size;
        if (num == 0) continue;
        BSSERT(num <= target);
        dp_bitset_base::apply_num_unsafe(num, bits, bitset_words);
    }
}

} // namespace sum_intersection_dp_bitset