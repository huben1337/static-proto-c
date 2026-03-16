#pragma once

#include <algorithm>
#include <cstdint>

#include "../QueuedField.hpp"
#include "../VariantLeafMeta.hpp"

#include "./sum_intersection_dp_bitset.hpp"
#include "../../../parser/lexer_types.hpp"

namespace layout::generation::variant_layout::perfect {

template <lexer::SIZE alignment>
requires (alignment != lexer::SIZE::SIZE_1)
[[nodiscard]] inline std::pair<uint16_t, uint64_t> find_st (
    dp_bitset_base::word_t* const current_bits,
    dp_bitset_base::word_t* const to_apply_bits,
    uint16_t applied_variants,
    const std::span<const VariantLeafMeta> variant_leaf_metas,
    const std::span<const QueuedField> queued_fields_buffer,
    const uint64_t max_used_space,
    const uint64_t min_offset
) {
    constexpr uint8_t alignement_bytes = alignment.byte_size();
    if (std::ranges::all_of(variant_leaf_metas, [](const VariantLeafMeta& e) {
        return e.required_spaces.get<alignment>() == 0;
    })) {
        BSSERT(false);
    }

    uint64_t target = max_used_space & ~(uint64_t{alignement_bytes} - 1);

    while (true) {
        for (; applied_variants != variant_leaf_metas.size(); applied_variants++) {
            const VariantLeafMeta& meta = variant_leaf_metas[applied_variants];
            if (meta.used_space < target) break;
            const uint64_t total_required_space = meta.required_spaces.total();
            if (total_required_space == 0) continue;
            const uint64_t already_applied_space = meta.used_space - total_required_space;
            const uint64_t word_offset = already_applied_space / dp_bitset_base::WORD_BITS;
            // CSSERT(total_required_space, <=, target);
            const auto to_apply_word_count = dp_bitset_base::bitset_word_count(total_required_space);
            console.log("total_required_space: ", total_required_space);
            sum_intersection_dp_bitset::generate_bits(to_apply_bits, to_apply_word_count, queued_fields_buffer, meta);
            const uint16_t sub_word_offset = already_applied_space - (word_offset * dp_bitset_base::WORD_BITS);
            if (sub_word_offset != 0) {
                dp_bitset_base::apply_num_unsafe(sub_word_offset, to_apply_bits, to_apply_word_count);
            }
            dp_bitset_base::and_merge(current_bits, to_apply_bits, total_required_space + 1, word_offset);
        }
        constexpr uint8_t alignement_bytes = alignment.byte_size();
        // check the target
        console.debug("trying ", target, " @ ", alignement_bytes, " mo: ", min_offset);
        if (dp_bitset_base::bit_at(current_bits, target)) {
            return {applied_variants, target};
        }
        if (target <= min_offset) {
            console.debug("could not find perfect layout at align", alignement_bytes);
            return {applied_variants, 0};
        }
        target -= alignement_bytes;
    }
}

} // namespace layout::generation::variant_layout::perfect