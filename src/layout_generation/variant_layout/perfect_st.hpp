#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include "../layout_data.hpp"

#include "./sum_intersection_dp_bitset.hpp"
#include "../../parser/lexer_types.hpp"

namespace variant_layout {

struct Layout : lexer::AlignMembersBase<uint64_t, lexer::SIZE::SIZE_8, Layout>{
    using AlignMembersBase::AlignMembersBase;

    template <lexer::SIZE alignment>
    [[nodiscard]] constexpr uint64_t get_space () const {
        if constexpr (alignment == lexer::SIZE::SIZE_8) {
            return align8;
        } else if constexpr (alignment == lexer::SIZE::SIZE_4) {
            return align4 - align8;
        } else if constexpr (alignment == lexer::SIZE::SIZE_2) {
            return align2 - align4;
        } else if constexpr (alignment == lexer::SIZE::SIZE_1) {
            return align1 - align2;
        } else {
            static_assert(false, "Invalid alignment");
        }
    }
    
    [[nodiscard]] static consteval Layout zero () { return AlignMembersBase::zero<Layout>(); }
};

namespace perfect {

namespace {

template <lexer::SIZE alignment, bool applied_all_variants>
[[nodiscard]] inline Layout find_st_ (
    std::span<VariantLeafMeta> variant_leaf_metas,
    std::span<const QueuedField> queued_fields_buffer,
    dp_bitset_base::word_t* current_bits,
    dp_bitset_base::word_t* to_apply_bits,
    uint64_t max_used_space,
    uint16_t applied_variants,
    Layout layout
);

template <lexer::SIZE alignment, bool applied_all_variants>
requires (alignment > lexer::SIZE::SIZE_1)
[[nodiscard]] inline Layout find_st_check_target_loop (
    const std::span<VariantLeafMeta> variant_leaf_metas,
    const std::span<const QueuedField> queued_fields_buffer,
    dp_bitset_base::word_t* const current_bits,
    dp_bitset_base::word_t* const to_apply_bits,
    const uint64_t max_used_space,
    uint16_t applied_variants,
    Layout layout,
    const uint64_t min_offset,
    uint64_t target
) {
    while (true) {
        if constexpr (!applied_all_variants) {
            // make sure all variants which reach to the same space are also conjected into the bitset
            while (true) {
                const VariantLeafMeta& meta = variant_leaf_metas[applied_variants];
                const uint64_t used_space = meta.required_space;
                if (used_space < target) break;
                auto to_apply_word_count = dp_bitset_base::bitset_word_count(target);
                sum_intersection_dp_bitset::generate_bits(to_apply_bits, to_apply_word_count, used_space, queued_fields_buffer, meta);
                dp_bitset_base::and_merge(current_bits, to_apply_bits, to_apply_word_count);
                applied_variants++;
                if (applied_variants == variant_leaf_metas.size()) {
                    return find_st_check_target_loop<alignment, true>(
                        variant_leaf_metas,
                        queued_fields_buffer,
                        current_bits,
                        to_apply_bits,
                        max_used_space,
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
        console.debug("trying ", target, " @ ", alignement_bytes, " mo: ", min_offset);
        if (dp_bitset_base::bit_at(current_bits, target)) {
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
            }
            return find_st_<alignment.next_smaller(), applied_all_variants>(
                variant_leaf_metas,
                queued_fields_buffer,
                current_bits,
                to_apply_bits,
                max_used_space,
                applied_variants,
                layout
            );
        }
        if (target <= min_offset) {
            console.debug("could not find perfect layout at align", alignement_bytes);
            return layout;
        }
        target -= alignement_bytes;
    }
}

template <lexer::SIZE alignment>
[[nodiscard]] inline uint64_t get_min_offset (
    const std::span<VariantLeafMeta> variant_leaf_metas,
    const Layout layout
) {
    if constexpr (alignment == lexer::SIZE::SIZE_8) {
        return std::ranges::max(
            std::views::transform(variant_leaf_metas, [](const VariantLeafMeta& e) { return e.used_spaces.align8; })
        );
    } else {
        return layout.get<alignment.next_bigger()>() + alignment.byte_size();
    }
}

template <lexer::SIZE alignment, bool applied_all_variants>
[[nodiscard]] inline Layout find_st_ (
    const std::span<VariantLeafMeta> variant_leaf_metas,
    const std::span<const QueuedField> queued_fields_buffer,
    dp_bitset_base::word_t* const current_bits,
    dp_bitset_base::word_t* const to_apply_bits,
    const uint64_t max_used_space,
    uint16_t applied_variants,
    Layout layout
) {
    if constexpr (alignment == lexer::SIZE::SIZE_1) {
        layout.align1 = max_used_space;
        return layout;
    } else {
        constexpr uint8_t alignement_bytes = alignment.byte_size();
        if constexpr (alignment != lexer::SIZE::SIZE_8) {
            static_assert(alignment != lexer::SIZE::SIZE_1);
            if (std::ranges::all_of(variant_leaf_metas, [](const VariantLeafMeta& e) {
                return e.used_spaces.get<alignment>() == 0;
            })) {
                layout.get<alignment>() = layout.get<alignment.next_bigger()>();
                goto empty;
            }
        }

        {
            const uint64_t min_offset = get_min_offset<alignment>(variant_leaf_metas, layout);
            if constexpr (alignment == lexer::SIZE::SIZE_8) {
                if (min_offset == 0) goto empty;
            }

            return find_st_check_target_loop<alignment, applied_all_variants>(
                variant_leaf_metas,
                queued_fields_buffer,
                current_bits,
                to_apply_bits,
                max_used_space,
                applied_variants,
                layout,
                min_offset,
                max_used_space & ~(uint64_t{alignement_bytes} - 1)
            );
        }

        empty: {
            console.debug("skipped empty align", alignement_bytes);
            return find_st_<alignment.next_smaller(), applied_all_variants>(
                variant_leaf_metas,
                queued_fields_buffer,
                current_bits,
                to_apply_bits,
                max_used_space,
                applied_variants,
                layout
            );
        }
    }
}

} // namespace



[[nodiscard]] inline Layout find_st (
    const std::span<VariantLeafMeta> variant_leaf_metas,
    const std::span<const QueuedField> queued_fields_buffer
) {
    BSSERT(variant_leaf_metas.size() >= 2, "find_perfect_variant_layout_st: variant_count shouldn't be less than 2")
    VariantLeafMeta biggest_variant_leaf_meta = variant_leaf_metas[0];

    auto biggest_word_count = dp_bitset_base::bitset_word_count(biggest_variant_leaf_meta.required_space);
    auto second_biggest_word_count = dp_bitset_base::bitset_word_count(variant_leaf_metas[1].required_space);

    const std::unique_ptr<dp_bitset_base::word_t[]> current_bits = std::make_unique_for_overwrite<dp_bitset_base::word_t[]>(biggest_word_count + second_biggest_word_count);

    sum_intersection_dp_bitset::generate_bits(
        current_bits.get(),
        biggest_word_count,
        biggest_variant_leaf_meta.required_space,
        queued_fields_buffer,
        biggest_variant_leaf_meta
    );

    return find_st_<lexer::SIZE::SIZE_8, false>(
        variant_leaf_metas,
        queued_fields_buffer,
        current_bits.get(),
        current_bits.get() + biggest_word_count,
        biggest_variant_leaf_meta.required_space,
        1,
        Layout::zero()
    );
}

} // namespace perfect

} // namespace variant_layout