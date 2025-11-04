#pragma once

#include <cstdint>
#include <cstdlib>
#include "./data.hpp"

#include "../subset_sum_solving/sum_intersection_dp_bitset.hpp"
#include "../subset_sum_solving/subset_sum_absorbed.hpp"
#include "../parser/lexer_types.hpp"

namespace {

template <lexer::SIZE alignment, bool applied_all_variants>
constexpr lexer::LeafSizes find_perfect_variant_layout_st_ (
    // const lexer::LeafSizes& max_fixed_leaf_sizes,
    VariantLeafMeta* variant_leaf_metas,
    sum_intersection_dp_bitset::word_t* current_bits,
    sum_intersection_dp_bitset::word_t* to_apply_buffer,
    uint64_t max_used_space,
    uint16_t variant_count,
    uint16_t applied_variants,
    lexer::LeafSizes layout
);

template <lexer::SIZE alignment, bool applied_all_variants>
requires (alignment > lexer::SIZE::SIZE_1)
constexpr lexer::LeafSizes find_perfect_variant_layout_st_check_target_loop (
    // const lexer::LeafSizes& max_fixed_leaf_sizes,
    VariantLeafMeta* const variant_leaf_metas,
    sum_intersection_dp_bitset::word_t* const current_bits,
    sum_intersection_dp_bitset::word_t* const to_apply_buffer,
    const uint64_t max_used_space,
    const uint16_t variant_count,
    uint16_t applied_variants,
    lexer::LeafSizes layout,
    const uint64_t min_offset,
    uint64_t target
) {
    while (true) {
        if constexpr (!applied_all_variants) {
            // make sure all variants which reach to the same space are also conjected into the bitset
            while (true) {
                const auto& meta = variant_leaf_metas[applied_variants];
                auto used_space = meta.required_space;
                if (used_space < target) break;
                auto to_apply_words = sum_intersection_dp_bitset::bitset_words_count(target);
                sum_intersection_dp_bitset::generate_bits(to_apply_buffer, to_apply_words, used_space, meta.fixed_leafs, meta.fixed_leaf_ends.align1);
                sum_intersection_dp_bitset::merge(current_bits, {to_apply_buffer, to_apply_words});
                applied_variants++;
                if (applied_variants == variant_count) {
                    return find_perfect_variant_layout_st_check_target_loop<alignment, true>(
                        // max_fixed_leaf_sizes,
                        variant_leaf_metas,
                        current_bits,
                        to_apply_buffer,
                        max_used_space,
                        variant_count,
                        applied_variants,
                        layout,
                        min_offset,
                        target
                    );
                }
            }
        }
        constexpr uint8_t alignement_bytes = alignment.byte_size();
        // check the target
        logger::debug("trying ", target, " @ ", alignement_bytes, " mo: ", min_offset);
        if (reinterpret_cast<uint8_t*>(current_bits)[target / 8] & (uint8_t{1} << (target % 8))) {
            layout.get<alignment>() = target;
            if (target == max_used_space) {
                if constexpr (alignment > lexer::SIZE::SIZE_1) {
                    layout.align1 = target;
                }
                if constexpr (alignment > lexer::SIZE::SIZE_2) {
                    layout.align2 = target;
                }
                if constexpr (alignment > lexer::SIZE::SIZE_4) {
                    layout.align4 = target;
                }
                return layout;
            } else {
                return find_perfect_variant_layout_st_<alignment.next_smaller(), applied_all_variants>(
                    // max_fixed_leaf_sizes,
                    variant_leaf_metas,
                    current_bits,
                    to_apply_buffer,
                    max_used_space,
                    variant_count,
                    applied_variants,
                    layout
                );
            }
        }
        if (target <= min_offset) {
            logger::debug("could not find perfect layout at align", alignement_bytes);
            return layout;
        }
        target -= alignement_bytes;
    }
}
template <lexer::SIZE alignment, bool applied_all_variants>
constexpr lexer::LeafSizes find_perfect_variant_layout_st_ (
    // const lexer::LeafSizes& max_fixed_leaf_sizes,
    VariantLeafMeta* const variant_leaf_metas,
    sum_intersection_dp_bitset::word_t* const current_bits,
    sum_intersection_dp_bitset::word_t* const to_apply_buffer,
    const uint64_t max_used_space,
    const uint16_t variant_count,
    uint16_t applied_variants,
    lexer::LeafSizes layout
) {
    if constexpr (alignment == lexer::SIZE::SIZE_1) {
        layout.align1 = max_used_space;
        return layout;
    } else {
        uint64_t min_offset;
        constexpr uint8_t alignement_bytes = alignment.byte_size();
        #if 0
        if constexpr (alignment == lexer::SIZE::SIZE_8) {
            uint64_t max_align8_space = 0;
            for (uint16_t i = 0; i < variant_count; i++) {
                const VariantLeafMeta& meta = variant_leaf_metas[i];
                max_align8_space = std::max(meta.used_spaces.align8, max_align8_space);
            }
            if (max_align8_space == 0) goto empty;
            min_offset = max_align8_space;
        } else if constexpr (alignment == lexer::SIZE::SIZE_4) {
            uint64_t max_align4_space = 0;
            for (uint16_t i = 0; i < variant_count; i++) {
                const VariantLeafMeta& meta = variant_leaf_metas[i];
                auto sponge_space = layout.align8 - meta.used_spaces.align8;
                if (sponge_space == 0) {
                    // Nothing is absorbed
                    max_align4_space = std::max(meta.used_spaces.align4, max_align4_space);
                    continue;
                }
                auto absorbed = subset_sum_absorbed::solve(
                    sponge_space,
                    meta.fixed_leafs + meta.fixed_leaf_ends.align8,
                    meta.fixed_leaf_ends.align4,
                    meta.fixed_leaf_ends.align1
                );
                auto align4_required_space = meta.used_spaces.align4 - absorbed;
                max_align4_space = std::max(align4_required_space, max_align4_space);
            }
            if (max_align4_space == 0) {
                layout.align4 = layout.align8;
                goto empty;
            }
            min_offset = layout.align8 + max_align4_space;
        } else if constexpr (alignment == lexer::SIZE::SIZE_2) {
            uint64_t max_align2_space = 0;
            for (uint16_t i = 0; i < variant_count; i++) {
                const VariantLeafMeta& meta = variant_leaf_metas[i];
                auto sponge_space = layout.align4 - meta.used_spaces.align8 - meta.used_spaces.align4;
                if (sponge_space == 0) {
                    // Nothing is absorbed
                    max_align2_space = std::max(meta.used_spaces.align2, max_align2_space);
                    continue;
                }
                auto absorbed = subset_sum_absorbed::solve(
                    sponge_space,
                    meta.fixed_leafs + meta.fixed_leaf_ends.align4,
                    meta.fixed_leaf_ends.align2,
                    meta.fixed_leaf_ends.align1
                );
                auto align2_required_space = meta.used_spaces.align2 - absorbed;
                max_align2_space = std::max(align2_required_space, max_align2_space);
            }
            if (max_align2_space == 0) {
                layout.align2 = layout.align4;
                goto empty;
            }
            min_offset = layout.align4 + max_align2_space;
        } else if constexpr (alignment == lexer::SIZE::SIZE_1) {
            layout.align1 = max_used_space;
            return layout;
        }
        #endif
        if constexpr (alignment == lexer::SIZE::SIZE_8) {
            uint64_t max_align8_space = 0;
            for (uint16_t i = 0; i < variant_count; i++) {
                const VariantLeafMeta& meta = variant_leaf_metas[i];
                max_align8_space = std::max(meta.used_spaces.align8, max_align8_space);
            }
            if (max_align8_space == 0) {
                goto empty;
            }
            logger::debug("alig8 min_offset: ", max_align8_space);
            min_offset = max_align8_space;
            // BSSERT(min_offset != max_used_space, "should be done in ez perfect layout");
        } else if constexpr (alignment == lexer::SIZE::SIZE_4) {
            uint64_t max_align4_space = 0;
            for (uint16_t i = 0; i < variant_count; i++) {
                const VariantLeafMeta& meta = variant_leaf_metas[i];
                max_align4_space = std::max(meta.used_spaces.align4, max_align4_space);
            }
            if (max_align4_space == 0) {
                layout.align4 = layout.align8;
                goto empty;
            }
            min_offset = layout.align8 + alignement_bytes;
        } else if constexpr (alignment == lexer::SIZE::SIZE_2) {
            uint64_t max_align2_space = 0;
            for (uint16_t i = 0; i < variant_count; i++) {
                const VariantLeafMeta& meta = variant_leaf_metas[i];
                max_align2_space = std::max(meta.used_spaces.align2, max_align2_space);
            }
            if (max_align2_space == 0) {
                layout.align2 = layout.align4;
                goto empty;
            }
            min_offset = layout.align4 + alignement_bytes;
        } else {
            static_assert(false);
        }
        // min_offset += lexer::get_size<alignment>(max_fixed_leaf_sizes);
        BSSERT(min_offset >= alignement_bytes, "Expected min_offset to be at least ")

        return find_perfect_variant_layout_st_check_target_loop<alignment, applied_all_variants>(
            // max_fixed_leaf_sizes,
            variant_leaf_metas,
            current_bits,
            to_apply_buffer,
            max_used_space,
            variant_count,
            applied_variants,
            layout,
            min_offset,
            max_used_space & ~(uint64_t{alignement_bytes} - 1)
        );

        empty: {
            logger::debug("skipped empty align", alignement_bytes);
            static_assert(alignment != lexer::SIZE::SIZE_1);
            return find_perfect_variant_layout_st_<alignment.next_smaller(), applied_all_variants>(
                // max_fixed_leaf_sizes,
                variant_leaf_metas,
                current_bits,
                to_apply_buffer,
                max_used_space,
                variant_count,
                applied_variants,
                layout
            );
        }
    }
}

}

constexpr lexer::LeafSizes find_perfect_variant_layout_st (
    VariantLeafMeta* const variant_leaf_metas,
    const uint16_t variant_count
) {
    BSSERT(variant_count >= 2, "find_perfect_variant_layout_st: variant_count shouldn't be less than 2")
    VariantLeafMeta biggest_variant_leaf_meta = variant_leaf_metas[0];

    auto biggest_bitset_words = sum_intersection_dp_bitset::bitset_words_count(biggest_variant_leaf_meta.required_space);
    auto second_biggest_bitset_words = sum_intersection_dp_bitset::bitset_words_count(variant_leaf_metas[1].required_space);
    gsl::owner<sum_intersection_dp_bitset::word_t*> word_buffer = sum_intersection_dp_bitset::allocate_bitset_words(
        sum_intersection_dp_bitset::bitset_words_count(biggest_bitset_words) + sum_intersection_dp_bitset::bitset_words_count(second_biggest_bitset_words)
    );

    sum_intersection_dp_bitset::generate_bits(
        word_buffer,
        biggest_bitset_words,
        biggest_variant_leaf_meta.required_space,
        biggest_variant_leaf_meta.fixed_leafs,
        biggest_variant_leaf_meta.fixed_leaf_ends.align1
    );

    lexer::LeafSizes layout = find_perfect_variant_layout_st_<lexer::SIZE::SIZE_8, false>(
        // max_fixed_leaf_sizes,
        variant_leaf_metas,
        word_buffer,
        word_buffer + biggest_bitset_words,
        biggest_variant_leaf_meta.required_space,
        variant_count,
        1,
        lexer::LeafSizes::zero()
    );
    std::free(word_buffer);
    return layout;   
}