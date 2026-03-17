#pragma once

#include <algorithm>
#include <cstdint>
#include <gsl/pointers>
#include <memory>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>

#include "../../../util/logger.hpp"
#include "../../FixedOffsets.hpp"
#include "../QueuedField.hpp"
#include "../PendingVariantFieldPacks.hpp"
#include "../field_queuing.hpp"
#include "./perfect_st.hpp"

namespace layout::generation::variant_layout {

constexpr uint16_t empty_chain_link = -1;

[[nodiscard]] std::unique_ptr<const uint16_t[]> generate_sum_subset_chains (
    const uint64_t target,
    const std::span<QueuedField> queued_fields_buffer,
    const estd::integral_range<uint16_t> queued_field_idxs
) {
    std::unique_ptr<uint16_t[]> sum_chains = std::make_unique_for_overwrite<uint16_t[]>(target + 1);
    sum_chains[0] = 0;
    std::uninitialized_fill_n(sum_chains.get() + 1, target, empty_chain_link);
    
    for (const uint16_t idx : queued_field_idxs) {
        const QueuedField& field = queued_fields_buffer[idx];
        uint64_t num = field.size;
        // CSSERT(num, <=, target);
        if (num == 0 || num > target) continue; // Skip leaf which has been marked as used.
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

        if (sum_chains[target] != empty_chain_link) return std::move(sum_chains);
    }

    std::unreachable();
}

template <SIZE pack_align>
[[nodiscard]] inline std::pair<uint16_t, uint64_t> apply_field (
    QueuedField& field,
    const uint64_t offset,
    uint16_t fixed_offset_idx,
    const std::span<FixedOffset> fixed_offsets,
    const std::span<FixedOffset> tmp_fixed_offsets
) {
    std::visit([&offset, &fixed_offset_idx, &fixed_offsets, &tmp_fixed_offsets]<typename T>(T& arg) {
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
        } else {
            static_assert(false);
        }
    }, field.info);
    const uint64_t field_size = field.size;
    BSSERT(field_size != 0);
    // Prevent a field from being used twice. The size is set to zero and therfore it will be skipped from here on out. [1]
    field.size = 0;
    return {fixed_offset_idx, offset + field_size};
}

struct FieldConsumer {
    uint64_t offset;
    uint16_t fixed_offset_idx;
    std::span<FixedOffset> fixed_offsets;
    std::span<FixedOffset> tmp_fixed_offsets;
    std::span<QueuedField> queued_fields_buffer;

    template <SIZE pack_align>
    void enqueue_for_level (QueuedField& field) {
        std::tie(fixed_offset_idx, offset) = apply_field<pack_align>(
            field,
            offset,
            fixed_offset_idx,
            fixed_offsets,
            tmp_fixed_offsets
        );
    }

    template <SIZE pack_align>
    void enqueue_for_level (const uint16_t idx) {
        enqueue_for_level<pack_align>(queued_fields_buffer[idx]);
    }
};

template <bool has_pre_selected>
using pre_selected_range_t = std::conditional_t<
    has_pre_selected,
    std::span<std::pair<uint16_t, uint64_t>>,
    estd::empty
>;

template <bool has_pre_selected>
using pre_selected_iterator_t = std::conditional_t<
    has_pre_selected,
    std::span<std::pair<uint16_t, uint64_t>>::const_iterator,
    estd::empty
>;

template <bool has_pre_selected>
using pre_selected_t = std::conditional_t<
    has_pre_selected,
    std::pair<uint16_t, uint64_t>,
    estd::empty
>;

template <SIZE alignment, bool has_pre_selected>
[[nodiscard]] inline std::pair<uint16_t, uint64_t> apply_solution (
    uint64_t chain_idx,
    const std::span<QueuedField> queued_fields_buffer,
    const std::unique_ptr<const uint16_t[]>& sum_chains,
    Fields<alignment>&& fields,
    VariantLeafMeta& meta,
    FieldConsumer field_consumer,
    pre_selected_iterator_t<has_pre_selected> pre_selected_begin = estd::empty{},
    const pre_selected_iterator_t<has_pre_selected> pre_selected_end = estd::empty{}
) {
    if constexpr (has_pre_selected) {
        BSSERT(pre_selected_begin != pre_selected_end);
    }

    pre_selected_t<has_pre_selected> next_pre_selected = *pre_selected_begin;

    do {
        // std::cout << "chain_idx: " << chain_idx << "\n";
        const uint16_t field_idx = sum_chains[chain_idx];
        QueuedField& field = queued_fields_buffer[field_idx];
        const uint64_t field_size = field.size;

        BSSERT(field_size != 0);

        const SIZE field_alignment = field.info.alignment();

        meta.left_fields.get<estd::discouraged>(field_alignment)--;

        if constexpr (alignment > SIZE::SIZE_1) {
            const SIZE largest_align = meta.left_fields.largest_align();
            
            if (largest_align < alignment) {
            // Not completely perfect since we get the field_idx and field again in here.
                meta.left_fields.get<estd::discouraged>(field_alignment)++;
                return largest_align.visit<std::pair<uint16_t, uint64_t>>(
                    make_size_range<SIZE::SIZE_1, alignment.next_smaller()>{},
                    [&]<SIZE next_alignment>() {
                        // TODO: Enqueue left fields.
                        make_size_range<next_alignment, alignment.next_smaller()>::foreach([]<SIZE v>(Fields<alignment>& fields) {
                            CSSERT(fields.template get<v>().idxs.size(), ==, 0);
                            // enqueueing_for_level<alignment>(field_consumer, fields.template get<v>().idxs);
                        }, fields);
                        CSSERT(fields.template get<alignment.next_smaller()>().idxs.size(), ==, 0);
                        return apply_solution<next_alignment, has_pre_selected>(
                            chain_idx,
                            queued_fields_buffer,
                            sum_chains,
                            std::move(fields).template extract<next_alignment>(),
                            meta,
                            field_consumer,
                            pre_selected_begin,
                            pre_selected_end
                        );
                    }
                );
            }
        }

        if constexpr (has_pre_selected) {
            while (next_pre_selected.first < field_idx) {
                QueuedField& pre_selected_field = field_consumer.queued_fields_buffer[next_pre_selected.first];
                BSSERT(pre_selected_field.size == 0);
                pre_selected_field.size = next_pre_selected.second;

                meta.required_spaces.get<estd::discouraged>(pre_selected_field.info.alignment()) -= pre_selected_field.size;
                add_field(field_consumer, next_pre_selected.first, fields, pre_selected_field.size);

                pre_selected_begin++;
                if (pre_selected_begin == pre_selected_end) {

                    meta.required_spaces.get<estd::discouraged>(field.info.alignment()) -= field_size;
                    add_field(field_consumer, field_idx, fields, field_size);

                    chain_idx -= field_size;
                    if (chain_idx == 0) goto done;
                    return apply_solution<alignment, false>(
                        chain_idx,
                        queued_fields_buffer,
                        sum_chains,
                        std::move(fields),
                        meta,
                        field_consumer,
                        estd::empty{},
                        estd::empty{}
                    );
                }
                next_pre_selected = *pre_selected_begin;
            }
        }

        meta.required_spaces.get<estd::discouraged>(field.info.alignment()) -= field_size;
        add_field(field_consumer, field_idx, fields, field_size);

        chain_idx -= field_size;
    } while (chain_idx > 0);

    done:
    if constexpr (has_pre_selected) {
        // I never had this fail but it should in some cases. TODO: Need to implement adding all the left over pre selected fields.
        BSSERT(pre_selected_begin == pre_selected_end);
    }
    // For variants which dont use all the layout space we can either dump all fields in a way which garuentees correct alignment, or better track what alignments still have fields to be aligned
    if constexpr (alignment > SIZE::SIZE_4) {
        CSSERT(fields.template get<SIZE::SIZE_4>().idxs.size(), ==, 0);
    }
    if constexpr (alignment > SIZE::SIZE_2) {
        CSSERT(fields.template get<SIZE::SIZE_2>().idxs.size(), ==, 0);
    }
    if constexpr (alignment > SIZE::SIZE_1) {
        enqueueing_for_level<alignment>(field_consumer, fields.template get<SIZE::SIZE_1>().idxs);
        // CSSERT(fields.get<SIZE::SIZE_1>().idxs.size(), ==, 0);
    }
    return {field_consumer.fixed_offset_idx, field_consumer.offset};
}

// O(n * t) | 0 < t < MAX_SUM
template <SIZE alignment, bool has_pre_selected>
[[nodiscard]] inline std::pair<uint16_t, uint64_t> solve_and_apply (
    const uint64_t layout_space,
    VariantLeafMeta& meta,
    uint64_t offset,
    uint16_t fixed_offset_idx,
    const std::span<FixedOffset> fixed_offsets,
    const std::span<FixedOffset> tmp_fixed_offsets,
    const std::span<QueuedField> queued_fields_buffer,
    const pre_selected_range_t<has_pre_selected> pre_selected = estd::empty{}
) {
    const uint64_t required_space = meta.required_spaces.get<alignment>();
    
    BSSERT(layout_space >= required_space, "The layout doesn't fulfill space requirements for "_sl + string_literal::from<alignment.byte_size()> + " byte aligned section of Variant "_sl, layout_space, " >= ", required_space);

    console.debug(
        "meta.used_space: ", meta.used_space,
        ", meta.required_spaces.total(): ", meta.required_spaces.total(),
        ", layout_space: ", layout_space,
        ", required_space: ", required_space,
        ", meta.left_fields: ", meta.left_fields);

    const uint64_t target = std::min<uint64_t>(layout_space - required_space, meta.required_spaces.total() - required_space);

    if (target != 0) {
        const SIZE largest_align = meta.left_fields.largest_align<alignment.next_smaller()>();
        // CSSERT(meta.left_fields.largest_align(), ==, largest_align); // Asserts that we count left fields perfectly
        CSSERT(largest_align, <, alignment); // Sanity check
        if ((largest_align < alignment.next_smaller())) {
            console.debug("largest align checks are not useless"); // :)
        }

        std::tie(fixed_offset_idx, offset) = largest_align.visit<std::pair<uint16_t, uint64_t>>(
            make_size_range<SIZE::SIZE_2, SIZE::SIZE_8>{},
            []<SIZE max_align>(
                const uint64_t target,
                FieldConsumer field_consumer,
                VariantLeafMeta& meta,
                const pre_selected_range_t<has_pre_selected> pre_selected,
                const std::unique_ptr<const uint16_t[]>& sum_chains
            ) {
                if constexpr (has_pre_selected) {
                    return apply_solution<alignment, true>(target, field_consumer.queued_fields_buffer, sum_chains, {}, meta, field_consumer, pre_selected.begin(), pre_selected.end());
                } else {
                    return apply_solution<alignment, false>(target, field_consumer.queued_fields_buffer, sum_chains, {}, meta, field_consumer);
                }
            },
            target,
            FieldConsumer{
                offset,
                fixed_offset_idx,
                fixed_offsets,
                tmp_fixed_offsets,
                queued_fields_buffer
            },
            meta,
            pre_selected,
            generate_sum_subset_chains(target, queued_fields_buffer, meta.field_idxs)
        );   

        BSSERT(meta.required_spaces.get<alignment>() == 0);
    } else {
        if constexpr (has_pre_selected) {
            for (const std::pair<uint16_t, uint64_t>& e : pre_selected) {
                QueuedField& field = queued_fields_buffer[e.first];
                BSSERT(field.size == 0);
                field.size = e.second;
                std::tie(fixed_offset_idx, offset) = apply_field<alignment>(
                    field,
                    offset,
                    fixed_offset_idx,
                    fixed_offsets,
                    tmp_fixed_offsets
                );
            }

            meta.required_spaces.get<alignment>() = 0;
        }
    }

    return {fixed_offset_idx, offset};
}


template <SIZE alignment>
requires (alignment == SIZE::SIZE_1)
[[nodiscard]] inline PendingVariantFieldPacks apply_layout_ (
    dp_bitset_base::word_t* const /*unused*/,
    dp_bitset_base::word_t* const /*unused*/,
    uint16_t /*unused*/,
    const std::span<QueuedField> queued_fields_buffer,
    const std::span<FixedOffset> fixed_offsets,
    const std::span<FixedOffset> tmp_fixed_offsets,
    const std::span<VariantLeafMeta> variant_leaf_metas,
    const uint64_t max_used_space,
    const uint16_t fixed_offset_idx_begin,
    const uint64_t prev_layout_end,
    PendingVariantFieldPacks packs
) {
    if (std::ranges::all_of(variant_leaf_metas, [](const VariantLeafMeta& e) {
        return e.required_spaces.get<SIZE::SIZE_1>() == 0; 
    })) {
        packs.get<SIZE::SIZE_1>() = {0, {0, 0}};
        return packs;
    }
    
    const uint64_t layout_end = max_used_space;
    const uint64_t layout_space = layout_end - prev_layout_end;

    console.debug("[apply_layout] alignemnt: ", SIZE::SIZE_1);

    uint16_t fixed_offset_idx = fixed_offset_idx_begin;
    uint64_t max_offset = 0;
    for (VariantLeafMeta& meta : variant_leaf_metas) {            
        uint64_t offset = 0;
        
        const uint16_t pre_selected_count = meta.left_fields.get<SIZE::SIZE_1>();
        if (pre_selected_count == 0) continue;
        
        uint64_t required = 0; // Only used for assert at this point

        for (const uint16_t& field_idx : meta.field_idxs) {
            // console.debug("pre select checking field at: ", field_idx);
            QueuedField& field = queued_fields_buffer[field_idx];
            BSSERT(field.size != ~uint64_t{0});

            if (field.size == 0) continue;
            
            BSSERT(field.info.alignment() == SIZE::SIZE_1);

            // console.debug("pre selected field with size: ", field.size, " from idx: ", field_idx);

            required += field.size;
            meta.left_fields.get<SIZE::SIZE_1>()--;

            std::tie(fixed_offset_idx, offset) = apply_field<SIZE::SIZE_1>(
                field,
                offset,
                fixed_offset_idx,
                fixed_offsets,
                tmp_fixed_offsets
            );

            field.size = 0; // Mark as tracked
        }

        BSSERT(meta.left_fields.get<SIZE::SIZE_1>() == 0);

        const auto required_space = meta.required_spaces.get<SIZE::SIZE_1>();

        CSSERT(required_space, ==, required);
        CSSERT(layout_space, ==, required_space);

        meta.required_spaces.get<SIZE::SIZE_1>() = 0;

        max_offset = std::max(offset, max_offset);
    }

    // state.template next_variant_pack<alignment>(max_offset, {fixed_offset_idx_begin, fixed_offset_idx});
    console.debug("packs.get<", SIZE::SIZE_1, ">() = {", max_offset, ", ", "{", fixed_offset_idx_begin, ", ", fixed_offset_idx, "}}" );
    packs.get<SIZE::SIZE_1>() = {max_offset, {fixed_offset_idx_begin, fixed_offset_idx}};
    return packs;
}

template <SIZE alignment>
requires (alignment != SIZE::SIZE_1)
[[nodiscard]] inline PendingVariantFieldPacks apply_layout_ (
    dp_bitset_base::word_t* const current_bits,
    dp_bitset_base::word_t* const to_apply_bits,
    uint16_t applied_variants,
    const std::span<QueuedField> queued_fields_buffer,
    const std::span<FixedOffset> fixed_offsets,
    const std::span<FixedOffset> tmp_fixed_offsets,
    const std::span<VariantLeafMeta> variant_leaf_metas,
    const uint64_t max_used_space,
    const uint16_t fixed_offset_idx_begin,
    const uint64_t prev_layout_end,
    PendingVariantFieldPacks packs
) {

    const uint64_t min_space = std::ranges::max(
        std::views::transform(variant_leaf_metas, [](const VariantLeafMeta& e) {
            return e.required_spaces.get<alignment>(); 
        })
    );

    if (min_space == 0) {
        packs.get<alignment>() = {0, {0, 0}};
        return apply_layout_<alignment.next_smaller()>(
            current_bits,
            to_apply_bits,
            applied_variants,
            queued_fields_buffer,
            fixed_offsets,
            tmp_fixed_offsets,
            variant_leaf_metas,
            max_used_space,
            fixed_offset_idx_begin,
            prev_layout_end,
            packs
        );
    }
    
    // TODO: In here sums of not already generated bitsets need to be corrected for,
    // since some values fields get their size set to 0
    // Solution: and_merge at an offset so that the highest reached sum is flush with meta.used_space. 
    uint64_t layout_end;
    std::tie(applied_variants, layout_end) = variant_layout::perfect::find_st<alignment>(
        current_bits,
        to_apply_bits,
        applied_variants,
        variant_leaf_metas,
        queued_fields_buffer,
        max_used_space,
        prev_layout_end + min_space
    );

    BSSERT(layout_end != 0, "Non perfect variant layouts disabled for now!");

    const uint64_t layout_space = layout_end - prev_layout_end;

    console.debug("[apply_layout] alignemnt: ", alignment);

    uint16_t fixed_offset_idx = fixed_offset_idx_begin;
    uint64_t max_offset = 0;
    for (VariantLeafMeta& meta : variant_leaf_metas) {            
        uint64_t offset = 0;
        
        const uint16_t pre_selected_count = meta.left_fields.get<alignment>();
        
        
        if (pre_selected_count > 0) {
            std::pair<uint16_t, uint64_t> pre_selected_buffer[pre_selected_count];
            std::span<std::pair<uint16_t, uint64_t>> pre_selected {pre_selected_buffer, pre_selected_count};
            uint16_t pre_slected_idx = 0;
            
            uint64_t required = 0; // Only used for assert at this point

            for (const uint16_t& field_idx : meta.field_idxs) {
                // console.debug("pre select checking field at: ", field_idx);
                QueuedField& field = queued_fields_buffer[field_idx];
                BSSERT(field.size != ~uint64_t{0});

                if constexpr (alignment != SIZE::SIZE_8) {
                    if (field.size == 0) continue;
                } else {
                    BSSERT(field.size != 0);
                }

                if (field.info.alignment() != alignment) continue;

                // console.debug("pre selected field with size: ", field.size, " from idx: ", field_idx);

                required += field.size;
                meta.left_fields.get<alignment>()--;

                pre_selected[pre_slected_idx++] = {field_idx, field.size};

                // Mark as tracked
                field.size = 0;
            }

            BSSERT(meta.left_fields.get<alignment>() == 0);
            BSSERT(meta.required_spaces.get<alignment>() == required);

            std::tie(fixed_offset_idx, offset) = solve_and_apply<alignment, true>(
                layout_space,
                meta,
                offset,
                fixed_offset_idx,
                fixed_offsets,
                tmp_fixed_offsets,
                queued_fields_buffer,
                pre_selected
            );
        } else {
            std::tie(fixed_offset_idx, offset) = solve_and_apply<alignment, false>(
                layout_space,
                meta,
                offset,
                fixed_offset_idx,
                fixed_offsets,
                tmp_fixed_offsets,
                queued_fields_buffer
            );
        }

        max_offset = std::max(offset, max_offset);
    }

    // state.template next_variant_pack<alignment>(max_offset, {fixed_offset_idx_begin, fixed_offset_idx});
    console.debug("packs.get<", alignment, ">() = {", max_offset, ", ", "{", fixed_offset_idx_begin, ", ", fixed_offset_idx, "}}" );
    packs.get<alignment>() = {max_offset, {fixed_offset_idx_begin, fixed_offset_idx}};

    return apply_layout_<alignment.next_smaller()>(
        current_bits,
        to_apply_bits,
        applied_variants,
        queued_fields_buffer,
        fixed_offsets,
        tmp_fixed_offsets,
        variant_leaf_metas,
        max_used_space,
        fixed_offset_idx,
        layout_end,
        packs
    );
}

[[nodiscard]] inline PendingVariantFieldPacks apply_layout (
    const std::span<QueuedField> queued_fields_buffer,
    const std::span<FixedOffset> fixed_offsets,
    const std::span<FixedOffset> tmp_fixed_offsets,
    const std::span<VariantLeafMeta> variant_leaf_metas,
    const uint16_t fixed_offset_idx_begin
) {
    BSSERT(variant_leaf_metas.size() >= 2, "find_perfect_variant_layout_st: variant_count shouldn't be less than 2")
    VariantLeafMeta biggest_variant_leaf_meta = variant_leaf_metas[0];

    auto biggest_word_count = dp_bitset_base::bitset_word_count(biggest_variant_leaf_meta.used_space);
    auto second_biggest_word_count = dp_bitset_base::bitset_word_count(variant_leaf_metas[1].used_space);

    const std::unique_ptr<dp_bitset_base::word_t[]> current_bits = std::make_unique_for_overwrite<dp_bitset_base::word_t[]>(biggest_word_count + second_biggest_word_count);

    sum_intersection_dp_bitset::generate_bits(
        current_bits.get(),
        biggest_word_count,
        queued_fields_buffer,
        biggest_variant_leaf_meta
    );

    return apply_layout_<SIZE::SIZE_8>(
        current_bits.get(),
        current_bits.get() + biggest_word_count,
        1,
        queued_fields_buffer,
        fixed_offsets,
        tmp_fixed_offsets,
        variant_leaf_metas,
        biggest_variant_leaf_meta.used_space,
        fixed_offset_idx_begin,
        0,
        {}
    );
}

} // namespace layout::generation::variant_layout
