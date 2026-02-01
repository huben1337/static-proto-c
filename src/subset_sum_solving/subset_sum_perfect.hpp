#pragma once

#include <cstdint>
#include <gsl/pointers>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "../util/logger.hpp"
#include "../variant_optimizer/data.hpp"
#include "../tvs.hpp"

namespace subset_sum_perfect {

enum class LINK_TYPE : uint8_t {
    EMPTY,
    ROOT,
    FIXED_LEAF,
    VARIANT_FIELD
};


static constexpr uint16_t empty_chain_link = -1;

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

        if (sum_chains[target] != empty_chain_link) return std::move(sum_chains);
    }

    std::unreachable();
}

template <lexer::SIZE pack_align>
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

    template <lexer::SIZE pack_align>
    void enqueue_for_level (QueuedField& field) {
        std::tie(fixed_offset_idx, offset) = apply_field<pack_align>(
            field,
            offset,
            fixed_offset_idx,
            fixed_offsets,
            tmp_fixed_offsets
        );
    }

    template <lexer::SIZE pack_align>
    void enqueue_for_level (const uint16_t idx) {
        enqueue_for_level<pack_align>(queued_fields_buffer[idx]);
    }
};


template <bool has_pre_selected>
using pre_selected_iterator_t = std::conditional_t<
    has_pre_selected,
    std::vector<std::pair<uint16_t, uint64_t>>::iterator,
    estd::empty
>;

template <bool has_pre_selected>
using pre_selected_t = std::conditional_t<
    has_pre_selected,
    std::pair<uint16_t, uint64_t>,
    estd::empty
>;

template <lexer::SIZE alignment, bool has_pre_selected>
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
    pre_selected_t<has_pre_selected> next_pre_selected = *pre_selected_begin++;

    do {
        // std::cout << "chain_idx: " << chain_idx << "\n";
        const uint16_t field_idx = sum_chains[chain_idx];
        QueuedField& field = queued_fields_buffer[field_idx];
        const uint64_t field_size = field.size;

        BSSERT(field_size != 0);

        const lexer::SIZE field_alignment = std::visit([]<typename T>(T& arg) -> lexer::SIZE {
            if constexpr (
                    std::is_same_v<SimpleField, T>
                || std::is_same_v<VariantFieldPack, T>
                || std::is_same_v<ArrayFieldPack, T>
            ) {
                return arg.get_alignment();
            } else {
                static_assert(false);
            }
        }, field.info);

        meta.left_fields.get(field_alignment)--;

        if (meta.left_fields.largest_align() < alignment) {
            console.warn("Could have downgraded target alignment in variant solver");
        }

        if constexpr (has_pre_selected) {
            while (next_pre_selected.first < field_idx) {
                QueuedField& pre_selected_field = field_consumer.queued_fields_buffer[next_pre_selected.first];
                BSSERT(pre_selected_field.size == 0);
                pre_selected_field.size = next_pre_selected.second;
                add_field(field_consumer, next_pre_selected.first, fields, next_pre_selected.second);
                if (pre_selected_begin == pre_selected_end) {
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
                next_pre_selected = *pre_selected_begin++;
            }
        }
        add_field(field_consumer, field_idx, fields, field_size);
        chain_idx -= field_size;
    } while (chain_idx > 0);

    done:
    if constexpr (has_pre_selected) {
        // I never had this fail but it should in some cases. TODO: Need to implement adding all the left over pre selected fields.
        BSSERT(pre_selected_begin == pre_selected_end);
    }
    // For variants which dont use all the layout space we can either dump all fields in a way which garuentees correct alignment, or better track what alignments still have fields to be aligned
    if constexpr (alignment > lexer::SIZE::SIZE_4) {
        CSSERT(fields.align4.idxs.size(), ==, 0);
    }
    if constexpr (alignment > lexer::SIZE::SIZE_2) {
        CSSERT(fields.align2.idxs.size(), ==, 0);
    }
    if constexpr (alignment > lexer::SIZE::SIZE_1) {
        enqueueing_for_level<alignment>(field_consumer, fields.align1.idxs);
        // CSSERT(fields.align1.idxs.size(), ==, 0);
    }
    return {field_consumer.fixed_offset_idx, field_consumer.offset};
}

// O(n * t) | 0 < t < MAX_SUM
template <lexer::SIZE alignment>
requires (alignment != lexer::SIZE::SIZE_1)
[[nodiscard]] inline std::pair<uint16_t, uint64_t> solve (
    const uint64_t target,
    FieldConsumer&& field_consumer,
    VariantLeafMeta& meta,
    std::vector<std::pair<uint16_t, uint64_t>>& pre_selected
) {
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    BSSERT(target != 0, "[subset_sum_perfect::solve] invalid target: ", target);
    //std::cout << "FINDING " << target << "\n";

    const std::unique_ptr<const uint16_t[]> sum_chains = generate_sum_subset_chains(target, field_consumer.queued_fields_buffer, meta.field_idxs);


    // constexpr std::pair<uint16_t, uint64_t> pre_selected_none {static_cast<uint16_t>(-1), {}};
    
    if (pre_selected.empty()) {
        return apply_solution<alignment, false>(target, field_consumer.queued_fields_buffer, sum_chains, {}, meta, field_consumer);
    }
    return apply_solution<alignment, true>(target, field_consumer.queued_fields_buffer, sum_chains, {}, meta, field_consumer, pre_selected.begin(), pre_selected.end());
}

[[nodiscard]] inline std::pair<uint16_t, uint64_t> solve (
    const uint64_t target,
    const lexer::SIZE max_align,
    FieldConsumer&& field_consumer,
    VariantLeafMeta& meta,
    std::vector<std::pair<uint16_t, uint64_t>>& pre_selected
) {
    #define SOLVE_CASE(ALIGN) \
    case ALIGN: return solve<ALIGN>(target, std::move(field_consumer), meta, pre_selected);

    switch (max_align) {
        SOLVE_CASE(lexer::SIZE::SIZE_2)
        SOLVE_CASE(lexer::SIZE::SIZE_4)
        SOLVE_CASE(lexer::SIZE::SIZE_8)
        default:
            BSSERT(false);
            std::unreachable();
    }
}

template <lexer::SIZE alignment>
requires (alignment != lexer::SIZE::SIZE_1)
[[nodiscard]] inline std::pair<uint16_t, uint64_t> apply_pre_selected (
    std::vector<std::pair<uint16_t, uint64_t>>& pre_selected,
    uint64_t offset,
    uint16_t fixed_offset_idx,
    const std::span<FixedOffset> fixed_offsets,
    const std::span<FixedOffset> tmp_fixed_offsets,
    const std::span<QueuedField> queued_fields_buffer
) {
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

    return {fixed_offset_idx, offset};
}

} // namespace subset_sum_perfect
