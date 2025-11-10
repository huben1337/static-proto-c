#pragma once

#include <cstdint>
#include <cstdlib>
#include <gsl/pointers>
#include <memory>
#include <utility>

#include "../util/logger.hpp"
#include "../variant_optimizer/data.hpp"

namespace subset_sum_perfect {

enum class LINK_TYPE : uint8_t {
    EMPTY,
    ROOT,
    FIXED_LEAF,
    VARIANT_FIELD
};

struct ChainLink {
    uint16_t idx;
    LINK_TYPE type;
    lexer::SIZE alignment;

    constexpr ChainLink (uint16_t idx, LINK_TYPE type, lexer::SIZE alignment)
    : idx(idx), type(type), alignment(alignment) {}
};

template <LINK_TYPE link_type, lexer::SIZE alignment>
requires (link_type == LINK_TYPE::FIXED_LEAF || link_type == LINK_TYPE::VARIANT_FIELD)
[[gnu::always_inline]] inline void add_num (
    const uint64_t target,
    ChainLink* const sum_chains,
    const uint64_t num,
    const uint16_t idx
) {
    BSSERT(num <= target);
    // #pragma clang loop vectorize(enable)
    for (uint64_t i = target - num; ;) {
        const ChainLink old_chain_entry = sum_chains[i];
        if (old_chain_entry.type != LINK_TYPE::EMPTY) {
            ChainLink& new_chain_entry = sum_chains[i + num];
            if (new_chain_entry.type == LINK_TYPE::EMPTY) {
                // console.debug("set sum_chains at: ", i + num, ", type: ", uint8_t(link_type));
                new_chain_entry = ChainLink{idx, link_type, alignment};
            }
        }
        if (i == 0) break;
        i--;
    }
}

[[gnu::always_inline]] inline std::pair<uint16_t, uint64_t> trace_solution (
    const uint64_t target,
    gsl::owner<ChainLink*> const sum_chains,
    FixedLeaf* const fixed_leafs_buffer,
    FixedOffset* const fixed_offsets,
    VariantField* const variant_fields_buffer,
    uint16_t* const idx_map,
    uint64_t offset,
    const lexer::SIZE pack_align,
    uint16_t ordered_idx
) {
    uint64_t chain_idx = target;
    do {
        // std::cout << "chain_idx: " << chain_idx << "\n";
        ChainLink link = sum_chains[chain_idx];
        if (link.type == LINK_TYPE::FIXED_LEAF) {
            FixedLeaf& leaf = fixed_leafs_buffer[link.idx];
            uint16_t map_idx = leaf.get_map_idx();
            idx_map[map_idx] = ordered_idx;
            fixed_offsets[ordered_idx] = {offset, pack_align};
            ordered_idx++;
            uint64_t leaf_size = leaf.get_size();
            offset += leaf_size;
            chain_idx -= leaf_size;
            // Prevent a leaf from being used twice. The size is set to zero and therfore it will be skipped from here on out.
            leaf.set_zero();
        } else {
            BSSERT(link.type == LINK_TYPE::VARIANT_FIELD && false);
            uint64_t& size = variant_fields_buffer[link.idx].sizes.get(link.alignment);
            offset += size;
            chain_idx -= size;
            size = 0;
        }
        
    } while (chain_idx > 0);

    std::free(sum_chains);
    return {ordered_idx, offset};
}

template <lexer::SIZE from_align>
[[nodiscard]] inline std::pair<uint16_t, uint64_t> solve_loop (
    const uint64_t target,
    const gsl::owner<ChainLink*> sum_chains,
    FixedLeaf* const fixed_leafs_buffer,
    const VariantLeafMeta& meta,
    const uint16_t fixed_leaf_begin_idx,
    FixedOffset* const fixed_offsets,
    const estd::integral_range<uint16_t> variant_field_idxs,
    VariantField* const variant_fields_buffer,
    uint16_t* idx_map,
    uint16_t ordered_idx,
    uint64_t offset,
    lexer::SIZE pack_align
) {

    for (uint16_t& idx : variant_field_idxs) {
        const VariantField& variant_field = variant_fields_buffer[idx];
        auto num = variant_field.sizes.get<from_align>();
        if (num == 0) continue;
        BSSERT(num <= target);
        add_num<LINK_TYPE::VARIANT_FIELD, from_align>(target, sum_chains, num, idx);

        if (sum_chains[target].type != LINK_TYPE::EMPTY) {
            return trace_solution(target, sum_chains, fixed_leafs_buffer, fixed_offsets, variant_fields_buffer, idx_map, offset, pack_align, ordered_idx);
        }
    }

    const uint16_t fixed_leaf_end_idx = meta.fixed_leafs_ends.get<from_align>();
    for (uint16_t leaf_idx = fixed_leaf_begin_idx; leaf_idx < fixed_leaf_end_idx; leaf_idx++) {
        uint64_t num = fixed_leafs_buffer[leaf_idx].get_size();
        if (num == 0) continue; // Skip leaf which has been marked as used.
        add_num<LINK_TYPE::FIXED_LEAF, from_align>(target, sum_chains, num, leaf_idx);

        if (sum_chains[target].type != LINK_TYPE::EMPTY) {
            return trace_solution(target, sum_chains, fixed_leafs_buffer, fixed_offsets, variant_fields_buffer, idx_map, offset, pack_align, ordered_idx);
        }
    }

    if constexpr (from_align == lexer::SIZE::SIZE_1) {
        BSSERT(false, "[subset_sum_perfect::solve] reached unreachable. target: ", target);
        std::unreachable();
    } else {
        return solve_loop<lexer::next_smaller_size<from_align>>(
            target,
            sum_chains,
            fixed_leafs_buffer,
            meta,
            fixed_leaf_end_idx,
            fixed_offsets,
            variant_field_idxs,
            variant_fields_buffer,
            idx_map,
            ordered_idx,
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
    FixedLeaf* const fixed_leafs_buffer,
    const VariantLeafMeta& meta,
    const uint16_t fixed_leaf_begin_idx,
    FixedOffset* const fixed_offsets,
    const estd::integral_range<uint16_t> variant_field_idxs,
    VariantField* const variant_fields_buffer,
    uint16_t* idx_map,
    uint16_t ordered_idx,
    uint64_t offset
) {
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    BSSERT(target != 0, "[subset_sum_perfect::solve] invalid target: ", target);
    //std::cout << "FINDING " << target << "\n";

    gsl::owner<ChainLink*> sum_chains = static_cast<ChainLink*>(std::malloc((target + 1) * sizeof(ChainLink)));
    BSSERT(sum_chains != nullptr, "[subset_sum_perfect::solve] allocation failed");
    sum_chains[0] = ChainLink{0, LINK_TYPE::ROOT, lexer::SIZE::SIZE_0};
    std::uninitialized_fill_n(sum_chains + 1, target, ChainLink{0, LINK_TYPE::EMPTY, lexer::SIZE::SIZE_0});
    
    return solve_loop<alignment>(
        target,
        sum_chains,
        fixed_leafs_buffer,
        meta,
        fixed_leaf_begin_idx,
        fixed_offsets,
        variant_field_idxs,
        variant_fields_buffer,
        idx_map,
        ordered_idx,
        offset,
        alignment
    );
}

} // namespace subset_sum_perfect
