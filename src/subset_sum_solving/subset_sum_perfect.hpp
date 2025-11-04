#pragma once

#include <cstdint>
#include <cstdlib>
#include <gsl/pointers>
#include <memory>
#include <utility>

#include "../util/logger.hpp"
#include "../variant_optimizer/data.hpp"

namespace subset_sum_perfect {

const uint64_t MAX_TARGET = (uint64_t{1} << 48) - 1;

// O(n * t) | 0 < t < MAX_SUM
[[nodiscard]] inline std::pair<uint16_t, uint64_t> solve (
    const uint64_t target,
    FixedLeaf* const nums,
    const uint16_t leaf_begin_idx,
    const uint16_t leaf_end_idx,
    FixedOffset* const fixed_offsets,
    uint16_t* idx_map,
    uint16_t ordered_idx,
    uint64_t offset,
    lexer::SIZE alignment
) {
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    BSSERT(target != 0 && target <= MAX_TARGET, "[subset_sum_perfect::solve] invalid target: ", target);
    //std::cout << "FINDING " << target << "\n";
    constexpr uint16_t NO_CHAIN_VAL = -1;

    gsl::owner<uint16_t*> sum_chains = static_cast<uint16_t*>(std::malloc((target + 1) * sizeof(uint16_t)));
    BSSERT(sum_chains != nullptr, "[subset_sum_perfect::solve] allocation failed");
    sum_chains[0] = uint16_t{0};
    std::uninitialized_fill_n(sum_chains + 1, target, NO_CHAIN_VAL);
    
    for (uint16_t leaf_idx = leaf_begin_idx; leaf_idx < leaf_end_idx; leaf_idx++) {
        uint64_t num = nums[leaf_idx].get_size();
        if (num == 0 || num > target) continue;
        #pragma clang loop vectorize(enable)
        for (uint64_t i = target - num; ;) {
            const uint16_t old_chain_entry = sum_chains[i];
            if (old_chain_entry != NO_CHAIN_VAL) {
                uint16_t& new_chain_entry = sum_chains[i + num];
                if (new_chain_entry == NO_CHAIN_VAL) {
                    new_chain_entry = leaf_idx;
                }
            }
            if (i == 0) break;
            i--;
        }
    

        if (sum_chains[target] != NO_CHAIN_VAL) {
            // std::cout << "SOLUTION!!! " << sum_chains[target] << "\n";
            uint64_t chain_idx = target;
            do {
                // std::cout << "chain_idx: " << chain_idx << "\n";
                uint16_t link = sum_chains[chain_idx];
                FixedLeaf& leaf = nums[link];
                uint16_t map_idx = leaf.get_map_idx();
                idx_map[map_idx] = ordered_idx;
                fixed_offsets[ordered_idx] = {offset, alignment};
                ordered_idx++;
                uint64_t leaf_size = leaf.get_size();
                offset += leaf_size;
                chain_idx -= leaf_size;
                // Prevent a leaf from being used twice. The size is set to zero and therfore it will be skipped from here on out.
                leaf.set_zero();
            } while (chain_idx > 0);

            std::free(sum_chains);
            return {ordered_idx, offset};
        }
    }

    BSSERT(false, "[subset_sum_perfect::solve] reached unreachable. target: ", target);
    std::unreachable();
}

} // namespace subset_sum_perfect
