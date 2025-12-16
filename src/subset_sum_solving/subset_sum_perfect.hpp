#pragma once

#include <cstdint>
#include <gsl/pointers>
#include <memory>
#include <span>
#include <utility>
#include <variant>

#include "../util/logger.hpp"
#include "../variant_optimizer/data.hpp"

namespace subset_sum_perfect {

enum class LINK_TYPE : uint8_t {
    EMPTY,
    ROOT,
    FIXED_LEAF,
    VARIANT_FIELD
};


static constexpr uint16_t empty_chain_link = -1;

template <lexer::SIZE from_align>
[[nodiscard]] inline std::pair<uint16_t, uint64_t> solve_loop (
    const uint64_t target,
    uint16_t* const sum_chains,
    const std::span<QueuedField> queued_fields_buffer,
    const VariantLeafMeta& meta,
    const uint16_t fixed_leaf_begin_idx,
    const std::span<FixedOffset> fixed_offsets,
    const std::span<FixedOffset> tmp_fixed_offsets,
    const std::span<uint16_t> idx_map,
    uint16_t fixed_offset_idx,
    uint64_t offset,
    const lexer::SIZE pack_align
) {
    const uint16_t fixed_leaf_end_idx = meta.ends.get<from_align>();
    for (const uint16_t idx : estd::integral_range{
        fixed_leaf_begin_idx,
        fixed_leaf_end_idx
    }) {
        const QueuedField& field = queued_fields_buffer[idx];
        uint64_t num = field.size;
        if (num == 0) continue; // Skip leaf which has been marked as used.
        BSSERT(num <= target);
        // #pragma clang loop vectorize(enable)
        for (uint64_t i = target - num; ;) {
            const uint16_t old_chain_entry = sum_chains[i];
            if (old_chain_entry != empty_chain_link) {
                uint16_t& new_chain_entry = sum_chains[i + num];
                if (new_chain_entry == empty_chain_link) {
                    // console.debug("set sum_chains at: ", i + num, ", type: ", uint8_t(link_type));
                    new_chain_entry = idx;
                }
            }
            if (i == 0) break;
            i--;
        }

        if (sum_chains[target] == empty_chain_link) continue;
        
        uint64_t chain_idx = target;
        do {
            // std::cout << "chain_idx: " << chain_idx << "\n";
            const uint16_t field_idx = sum_chains[chain_idx];
            QueuedField& field = queued_fields_buffer[field_idx];
            std::visit([&fixed_offsets, &tmp_fixed_offsets, &fixed_offset_idx, &offset, &pack_align]<typename T>(T& arg) {
                if constexpr (std::is_same_v<SimpleField, T>) {
                    const uint16_t map_idx = arg.map_idx;
                    const FixedOffset fo {offset, map_idx, pack_align};
                    console.debug("(ssp) fixed_offsets[", fixed_offset_idx, "] = ", fo);
                    FixedOffset& out = fixed_offsets[fixed_offset_idx];
                    BSSERT(out == FixedOffset::empty());
                    out = fo;
                    fixed_offset_idx++;
                } else if constexpr (std::is_same_v<ArrayFieldPack, T> || std::is_same_v<VariantFieldPack, T>) {
                    const estd::integral_range<uint16_t>& tmp_fixed_offset_idxs = arg.tmp_fixed_offset_idxs;
                    console.debug("tmp_fixed_offsets[", *tmp_fixed_offset_idxs.begin(), " .. ", *tmp_fixed_offset_idxs.end(), "] = FixedOffset::empty() (ssp)");
                    for (const uint16_t tmp_idx : tmp_fixed_offset_idxs) {
                        FixedOffset& tmp = tmp_fixed_offsets[tmp_idx];
                        const FixedOffset fo {tmp.offset + offset, tmp.map_idx, tmp.pack_align};
                        console.debug("(ssp) fixed_offsets[", fixed_offset_idx, "] = ", fo);
                        FixedOffset& out = fixed_offsets[fixed_offset_idx];
                        BSSERT(out == FixedOffset::empty(), fixed_offset_idx);
                        out = fo;
                        tmp = FixedOffset::empty();
                        fixed_offset_idx++;
                    }
                } else if constexpr (std::is_same_v<SkippedField, T>) {
                    std::unreachable();
                } else {
                    static_assert(false);
                }
            }, field.info);
            const uint64_t size = field.size;
            // Prevent a field from being used twice. The size is set to zero and therfore it will be skipped from here on out.
            field.size = 0;
            offset += size;
            chain_idx -= size;
        } while (chain_idx > 0);

        return {fixed_offset_idx, offset};
    }

    if constexpr (from_align == lexer::SIZE::SIZE_1) {
        BSSERT(false, "[subset_sum_perfect::solve] reached unreachable. target: ", target);
        std::unreachable();
    } else {
        return solve_loop<lexer::next_smaller_size<from_align>>(
            target,
            sum_chains,
            queued_fields_buffer,
            meta,
            fixed_leaf_end_idx,
            fixed_offsets,
            tmp_fixed_offsets,
            idx_map,
            fixed_offset_idx,
            offset,
            pack_align
        );
    }
    
}

// O(n * t) | 0 < t < MAX_SUM
template <lexer::SIZE alignment>
requires (alignment != lexer::SIZE::SIZE_1)
[[nodiscard]] inline std::pair<uint16_t, uint64_t> solve (
    const uint64_t target,
    const std::span<QueuedField> queued_fields_buffer,
    const VariantLeafMeta& meta,
    const uint16_t fixed_leaf_begin_idx,
    const std::span<FixedOffset> fixed_offsets,
    const std::span<FixedOffset> tmp_fixed_offsets,
    const std::span<uint16_t> idx_map,
    const uint16_t fixed_offset_idx,
    const uint64_t offset
) {
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    BSSERT(target != 0, "[subset_sum_perfect::solve] invalid target: ", target);
    //std::cout << "FINDING " << target << "\n";

    const std::unique_ptr<uint16_t[]> sum_chains = std::make_unique_for_overwrite<uint16_t[]>(target + 1);
    BSSERT(sum_chains != nullptr, "[subset_sum_perfect::solve] allocation failed");
    sum_chains[0] = 0;
    std::uninitialized_fill_n(sum_chains.get() + 1, target, empty_chain_link);
    
    return solve_loop<alignment>(
        target,
        sum_chains.get(),
        queued_fields_buffer,
        meta,
        fixed_leaf_begin_idx,
        fixed_offsets,
        tmp_fixed_offsets,
        idx_map,
        fixed_offset_idx,
        offset,
        alignment
    );
}

} // namespace subset_sum_perfect
