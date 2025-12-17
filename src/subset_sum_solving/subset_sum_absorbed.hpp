#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <gsl/pointers>
#include <memory>
#include <utility>

#include "../estd/integral_pair.hpp"
#include "../util/logger.hpp"
#include "../variant_optimizer/data.hpp"

namespace subset_sum_absorbed {

const size_t TARGET_MAX_BITS = 48;
const uint64_t MAX_TARGET = (uint64_t{1} << TARGET_MAX_BITS) - 1;

struct chain_link : estd::u48_u16_pair {

    constexpr chain_link () = default;
    constexpr explicit chain_link (uint64_t data) : u48_u16_pair(data) {}
    constexpr explicit chain_link (uint64_t chain_idx, bool do_count) : u48_u16_pair(chain_idx, static_cast<uint16_t>(do_count)) {}

    [[nodiscard]] constexpr uint64_t chain_idx () const { return get_u48(); }

    [[nodiscard]] constexpr bool do_count () const { return get_u16() != 0U; }
};

constexpr chain_link NO_CHAIN_VAL = chain_link{static_cast<uint64_t>(-1)};

template <bool do_count>
[[gnu::always_inline]] inline void solve_loop (const uint64_t target, uint64_t num, chain_link* sum_chains) {
    #pragma clang loop vectorize(enable)
    for (uint64_t i = target - num; ;) {
        const chain_link old_chain_entry = sum_chains[i];
        if (old_chain_entry != NO_CHAIN_VAL) {
            chain_link& new_chain_entry = sum_chains[i + num];
            if (new_chain_entry == NO_CHAIN_VAL) {
                //std::cout << "possible: " << i + num << " with prev: " << i << "\n";
                new_chain_entry = chain_link{i, do_count};
            }
        }
        if (i == 0) break;
        i--;
    }
}

[[nodiscard, gnu::always_inline]] inline uint64_t count_absorbed (uint64_t chain_idx, chain_link* sum_chains) {
    uint64_t result = 0;
    do {
        // std::cout << "chain_idx: " << chain_idx << "\n";
        chain_link link = sum_chains[chain_idx];
        uint64_t next_chain_idx = link.chain_idx();
        uint64_t delta = chain_idx - next_chain_idx;
        if (link.do_count()) {
            result += delta;
        }
        chain_idx = next_chain_idx;
    } while (chain_idx > 0);

    return result;
}

// O(n * t) | 0 < t < MAX_SUM
[[nodiscard]] inline uint64_t solve (const uint64_t target, const FixedLeaf* const nums, const uint16_t counted_num_end, const uint16_t num_count) {
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    BSSERT(target != 0 && target <= MAX_TARGET, "[subset_sum_absorbed::solve] invalid target: ", target);
    //std::cout << "FINDING " << target << "\n";
    const std::unique_ptr<chain_link[]> sum_chains = std::make_unique_for_overwrite<chain_link[]>(target + 1);
    BSSERT(sum_chains != nullptr, "[subset_sum_absorbed::solve] allocation failed");
    sum_chains[0] = chain_link{0};
    std::uninitialized_fill_n(sum_chains.get() + 1, target, NO_CHAIN_VAL);
    
    const FixedLeaf* const it_end = nums + counted_num_end;
    for (const FixedLeaf* it = nums; nums != it_end; it++) {
        uint64_t num = it->get_size();
        if (num == 0) continue;
        BSSERT(num <= target);
        solve_loop<true>(target, num, sum_chains.get());   

        if (sum_chains[target] != NO_CHAIN_VAL) {
            return count_absorbed(target, sum_chains.get());
        }
    }
    const FixedLeaf* const rest_it_end = nums + num_count;
    for (const FixedLeaf* it = it_end; nums != rest_it_end; it++) {
        uint64_t num = it->get_size();
        if (num == 0) continue;
        BSSERT(num <= target);
        solve_loop<false>(target, num, sum_chains.get());   

        if (sum_chains[target] != NO_CHAIN_VAL) {
            return count_absorbed(target, sum_chains.get());
        }
    }

    BSSERT(false, "[subset_sum_absorbed::solve] reached unreachable lol")
    std::unreachable();
}

} // namespace subset_sum_absorbed
