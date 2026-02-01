#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gsl/pointers>
#include <gsl/util>
#include <memory>
#include <ranges>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "./parser/lexer_types.hpp"
#include "./estd/ranges.hpp"
#include "./container/memory.hpp"
#include "./util/logger.hpp"
#include "./helper/internal_error.hpp"
#include "./helper/alloca.hpp"
#include "./variant_optimizer/perfect_st.hpp"
#include "estd/empty.hpp"
#include "estd/utility.hpp"
#include "subset_sum_solving/subset_sum_perfect.hpp"
#include "util/string_literal.hpp"
#include "variant_optimizer/data.hpp"
#include "./tvs.hpp"


namespace generate_offsets {

[[nodiscard]] constexpr lexer::LeafCounts::Counts create_positions (const lexer::LeafCounts::Counts& counts, uint16_t offset = 0) {
    return {
        gsl::narrow_cast<uint16_t>(offset + counts.align8 + counts.align4 + counts.align2),
        gsl::narrow_cast<uint16_t>(offset + counts.align8 + counts.align4),
        gsl::narrow_cast<uint16_t>(offset + counts.align8),
        offset
    };
}
[[nodiscard]] constexpr lexer::LeafCounts::Counts create_positions (const lexer::LeafCounts& leaf_counts, uint16_t offset = 0) {
    return create_positions(leaf_counts.counts(), offset);
}

template <typename NextType, typename State, bool in_array, bool in_fixed_size>
struct TypeVisitor {
    using next_type_t = NextType;
    using result_t = lexer::Type::VisitResult<next_type_t>;
    using state_t = State;
    
    constexpr explicit TypeVisitor (
        const State& state
    ) :
    state(state)
    {}

    State state;

    template <lexer::FIELD_TYPE field_type>
    void on_simple () const {
        console.debug("[on_simple] field_type: ", (uint8_t)field_type);
        constexpr lexer::SIZE alignment = lexer::get_type_alignment<field_type>();
        state.template next_simple<alignment>();
    }

    void on_bool     () const { on_simple<lexer::FIELD_TYPE::BOOL   >(); }
    void on_uint8    () const { on_simple<lexer::FIELD_TYPE::UINT8  >(); }
    void on_uint16   () const { on_simple<lexer::FIELD_TYPE::UINT16 >(); }
    void on_uint32   () const { on_simple<lexer::FIELD_TYPE::UINT32 >(); }
    void on_uint64   () const { on_simple<lexer::FIELD_TYPE::UINT64 >(); }
    void on_int8     () const { on_simple<lexer::FIELD_TYPE::INT8   >(); }
    void on_int16    () const { on_simple<lexer::FIELD_TYPE::INT16  >(); }
    void on_int32    () const { on_simple<lexer::FIELD_TYPE::INT32  >(); }
    void on_int64    () const { on_simple<lexer::FIELD_TYPE::INT64  >(); }
    void on_float32  () const { on_simple<lexer::FIELD_TYPE::FLOAT32>(); }
    void on_float64  () const { on_simple<lexer::FIELD_TYPE::FLOAT64>(); }

    void on_fixed_string (const lexer::FixedStringType* const fixed_string_type) const {
        const uint32_t length = fixed_string_type->length;
        state.template next_simple<lexer::SIZE::SIZE_1>(length);
    }

    void on_string (const lexer::StringType* const string_type) const {
        if constexpr (in_array) {
            INTERNAL_ERROR("Variable length strings in arrays not supported");
        } else if constexpr (std::is_same_v<State, FixedVariantLevel::State>) {
            INTERNAL_ERROR("Variable length strings in fixed variant are nonsensical");
        } else {
            const lexer::SIZE stored_size_size = string_type->stored_size_size;
            state.template next_simple_var<lexer::SIZE::SIZE_1>();

            state.next_simple(stored_size_size, 1);
        }
    }

    template <lexer::SIZE alignment>
    void add_fixed_array_packs(
        const FixedArrayLevel::State& level_state,
        const uint16_t pack_info_base_idx,
        const uint16_t fixed_offset_idx_begin,
        const uint64_t last_offset,
        const uint32_t array_length
    ) const {
        if constexpr (alignment != lexer::SIZE::SIZE_8) {
            level_state.mutable_state.level().fixed_offset_idx = fixed_offset_idx_begin;
            level_state.try_solve_queued_for_align<alignment>();
        }
        const uint16_t fixed_offset_idx_end = level_state.mutable_state.level().fixed_offset_idx;
        const uint64_t current_offfset = level_state.mutable_state.level().current_offset;

        state.template next_array_pack<alignment>(
            (current_offfset - last_offset) * array_length,
            {fixed_offset_idx_begin, fixed_offset_idx_end},
            pack_info_base_idx + alignment.value
        );

        if constexpr (alignment != lexer::SIZE::SIZE_1) {
            // const uint16_t fixed_offset_idx_begin = state.get_fixed_offset_idx();
            // level_state.mutable_state.level().fixed_offset_idx = fixed_offset_idx_begin;

            add_fixed_array_packs<lexer::next_smaller_size<alignment>>(
                level_state,
                pack_info_base_idx,
                state.get_fixed_offset_idx(),
                current_offfset,
                array_length
            );
        }
    }

    [[nodiscard]] result_t on_fixed_array(lexer::ArrayType* const fixed_array_type) const {
        const uint16_t pack_info_base_idx = state.next_pack_info_base_idx();
        fixed_array_type->pack_info_base_idx = pack_info_base_idx;
        
        const uint16_t fixed_offset_idx_begin = state.get_fixed_offset_idx();

        console.debug("[on_fixed_array] fixed_offset_idx_begin: ", fixed_offset_idx_begin);

        FixedArrayLevel::MutableState::Level level_mutable_state {
            fixed_offset_idx_begin,
            state.mutable_state.level().tmp_fixed_offset_idx
        };

        TypeVisitor<next_type_t, FixedArrayLevel::State, true, in_fixed_size> visitor {
            FixedArrayLevel::State{
                FixedArrayLevel::ConstState{
                    state.const_state.shared(),
                    FixedArrayLevel::ConstState::Level{
                        pack_info_base_idx
                    }
                },
                FixedArrayLevel::MutableState{
                    state.mutable_state.shared(),
                    level_mutable_state
                }
            }
        };
        result_t result = fixed_array_type->inner_type()->visit(visitor);

        add_fixed_array_packs<lexer::SIZE::SIZE_8>(
            visitor.state,
            pack_info_base_idx,
            fixed_offset_idx_begin,
            0,
            fixed_array_type->length
        );

        return result;
    }


    [[nodiscard]] result_t on_array (const lexer::ArrayType* const /**/) const {
        INTERNAL_ERROR("Dynamic array not supported yet");
        /* if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
            const lexer::SIZE stored_size_size = array_type->stored_size_size;

            auto result = array_type->inner_type()->visit(TypeVisitor<next_type_t, TopLevel::State, true, false>{
                state
            });

            state.next_simple(stored_size_size, stored_size_size.byte_size());

            state.mutable_state.level().current_size_leaf_idx++;

            return result;
        } */
    }

    template <lexer::SIZE alignment>
    void apply_layout (
        const std::span<QueuedField> queued_fields_buffer,
        const variant_layout::Layout& layout,
        const uint16_t variant_count,
        VariantLeafMeta* const variant_leaf_metas,
        const uint16_t fixed_offset_idx_begin,
        lexer::AlignMembersBase<std::pair<uint64_t, estd::integral_range<uint16_t>>> packs
    ) const {
        if constexpr (alignment == lexer::SIZE::SIZE_1) {
            BSSERT(layout.align1 != 0, "This should not happen. layout for alignemnt 1 is always defined as long as higher alignments are defined");
        } else {
            // TODO: This doesnt't respect variants where some algiments simply are not present.
            if (layout.get<alignment>() == 0) {
                if constexpr (alignment != lexer::SIZE::SIZE_8) {
                    // We could template this with something like: bool had_fields. Then we could simply disallow 0 layout offset when we have already had fields.
                    BSSERT(layout.get<lexer::next_bigger_size<alignment>>() == 0, "Non perfect variant layouts disabled for now!");
                }
                // state.template next_variant_pack<alignment>(0, {0, 0});
                packs.get<alignment>() = {0, {0, 0}};
                return apply_layout<lexer::next_smaller_size<alignment>>(
                    queued_fields_buffer,
                    layout,
                    variant_count,
                    variant_leaf_metas,
                    fixed_offset_idx_begin,
                    packs
                );
            }
        }

        const std::span<FixedOffset>& fixed_offsets = state.const_state.shared().fixed_offsets;
        const std::span<FixedOffset>& tmp_fixed_offsets = state.const_state.shared().tmp_fixed_offsets;
        
        // const uint16_t ordered_idx_begin = ordered_idx;
        console.debug("[apply_layout] alignemnt: ", alignment.byte_size());
        //const uint16_t fixed_offset_idx_begin = state.get_fixed_offset_idx();
        uint16_t fixed_offset_idx = fixed_offset_idx_begin;
        uint64_t max_offset = 0;
        for (VariantLeafMeta& meta : std::ranges::subrange{
            variant_leaf_metas,
            variant_leaf_metas + variant_count
        }) {            
            uint64_t required = 0;
            uint64_t offset = 0;

            std::conditional_t<alignment != lexer::SIZE::SIZE_1, 
                std::vector<std::pair<uint16_t, uint64_t>>,
                estd::empty> pre_selected;

            // TODO: Do this with stack allocation only
            if constexpr (alignment != lexer::SIZE::SIZE_1) {
                pre_selected.reserve(meta.left_fields.get<alignment>());
            }

            for (const uint16_t& field_idx : meta.field_idxs) {
                // console.debug("pre select checking field at: ", field_idx);
                QueuedField& field = queued_fields_buffer[field_idx];
                BSSERT(field.size != ~uint64_t{0});
                if constexpr (alignment != lexer::SIZE::SIZE_8) {
                    if (field.size == 0) continue;
                } else {
                    BSSERT(field.size != 0);
                }
                const bool not_target_alignment = std::visit([]<typename T>(T& arg) -> bool {
                    if constexpr (
                           std::is_same_v<SimpleField, T>
                        || std::is_same_v<VariantFieldPack, T>
                        || std::is_same_v<ArrayFieldPack, T>
                    ) {
                        return arg.get_alignment() != alignment;
                    } else {
                        static_assert(false);
                    }
                }, field.info);
                if (not_target_alignment) continue;
                console.debug("pre selected field with size: ", field.size, " from idx: ", field_idx);
                required += field.size;
                if constexpr (alignment != lexer::SIZE::SIZE_1) {
                    pre_selected.emplace_back(field_idx, field.size);
                    // Mark as tracked
                    field.size = 0;
                } else {
                    // Field gets marked as tracked in this called function. But that doesnt really matter since we are done after applying the layout for alignment 1
                    std::tie(fixed_offset_idx, offset) = subset_sum_perfect::apply_field<alignment>(
                        field,
                        offset,
                        fixed_offset_idx,
                        fixed_offsets,
                        tmp_fixed_offsets
                    );
                }
                // TODO: We do not need to track this for alinemnt 1.
                meta.left_fields.get<alignment>()--;
            }
            BSSERT(meta.left_fields.get<alignment>() == 0);
            // console.debug("[apply_layout] alignemnt: ", alignment.byte_size(), " i: ", i);
            const uint64_t layout_space = layout.get_space<alignment>();
            
            BSSERT(layout_space >= required, "The layout doesn't fulfill space requirements for "_sl + string_literal::from<alignment.byte_size()> + " byte aligned section of Variant "_sl, layout_space, " >= ", required);

            if constexpr (alignment != lexer::SIZE::SIZE_1) {
                const auto used_space = meta.used_spaces.get<alignment>();
                console.debug("meta.required_space: ", meta.required_space, ", layout_space: ", layout_space, ", used_space: ", used_space, ", required: ", required);
                const uint64_t target = std::min<uint64_t>(layout_space - required, meta.required_space - required);
                console.debug("meta.left_fields: ", meta.left_fields);
                const lexer::SIZE largest_align = meta.left_fields.largest_align<alignment.next_smaller()>();
                // CSSERT(meta.left_fields.largest_align(), ==, largest_align); // Asserts that we count left fields perfectly
                CSSERT(largest_align, <, alignment); // Sanity check

                if ((largest_align < alignment.next_smaller())) {
                    console.debug("largest align checks are not useless");
                }

                if (target != 0) {
                    std::tie(fixed_offset_idx, offset) = subset_sum_perfect::solve(
                        target,
                        largest_align,
                        {
                            offset,
                            fixed_offset_idx,
                            fixed_offsets,
                            tmp_fixed_offsets,
                            queued_fields_buffer
                        },
                        meta,
                        pre_selected
                    );
                    // console.debug("target != 0  ", fixed_offset_idx, " ", offset);
                } else {
                    std::tie(fixed_offset_idx, offset) = subset_sum_perfect::apply_pre_selected<alignment>(
                        pre_selected,
                        offset,
                        fixed_offset_idx,
                        fixed_offsets,
                        tmp_fixed_offsets,
                        queued_fields_buffer
                    );
                }

            }

            meta.required_space -= offset;
            max_offset = std::max(offset, max_offset);
        }

        // state.template next_variant_pack<alignment>(max_offset, {fixed_offset_idx_begin, fixed_offset_idx});
        packs.get<alignment>() = {max_offset, {fixed_offset_idx_begin, fixed_offset_idx}};

        if constexpr (alignment == lexer::SIZE::SIZE_1) {
            state.template next_variant_pack<lexer::SIZE::SIZE_8>(packs.get<lexer::SIZE::SIZE_8>());
            state.template next_variant_pack<lexer::SIZE::SIZE_4>(packs.get<lexer::SIZE::SIZE_4>());
            state.template next_variant_pack<lexer::SIZE::SIZE_2>(packs.get<lexer::SIZE::SIZE_2>());
            state.template next_variant_pack<lexer::SIZE::SIZE_1>(packs.get<lexer::SIZE::SIZE_1>());
        } else {
            static constexpr lexer::SIZE next_alignment = lexer::next_smaller_size<alignment>;
            return apply_layout<next_alignment>(
                queued_fields_buffer,
                layout,
                variant_count,
                variant_leaf_metas,
                fixed_offset_idx,
                packs
            );
        }
    }

    void on_fixed_variant (lexer::FixedVariantType* const fixed_variant_type) const {
        if constexpr (in_array && !in_fixed_size) {
            INTERNAL_ERROR("Fixed variants in variabl sized arrays not supported yet");
        } else {

        const uint16_t variant_count = fixed_variant_type->variant_count;

        if (variant_count <= UINT8_MAX) {
            state.template next_simple<lexer::SIZE::SIZE_1>();
        } else {
            state.template next_simple<lexer::SIZE::SIZE_2>();
        }

        uint16_t fixed_field_packs_total = 0;
        lexer::LeafCounts fixed_leaf_counts_total = lexer::LeafCounts::zero();
        for (uint16_t i = 0; i < variant_count; i++) {
            const lexer::FixedVariantTypeMeta& meta = fixed_variant_type->type_metas()[i];
            fixed_field_packs_total += (meta.level_fixed_variants + meta.level_fixed_arrays) * 4;
            fixed_leaf_counts_total += meta.level_fixed_leafs;
        }
        const uint16_t total_fixed_leafs = fixed_leaf_counts_total.total();

        // const uint16_t level_fixed_idx_start = state.mutable_state.shared().fixed_offset_idx_base;
        // const uint16_t level_fixed_idx_end = level_fixed_idx_start + total_fixed_leafs;
        // state.mutable_state.shared().fixed_offset_idx_base = level_fixed_idx_end;
        // console.debug("level fixed_idx range: ", level_fixed_idx_start, " - ", level_fixed_idx_end);

        uint64_t max_used_space = 0;
        // variant_count > 0 is asserted during lexing
        VariantLeafMeta variant_leaf_metas[variant_count];
        ALLOCA_SAFE_SPAN(queued_fields_buffer, QueuedField, total_fixed_leafs + fixed_field_packs_total);
        uint16_t queued_fields_base = 0;

        uint16_t tmp_fixed_offset_idx_base = state.mutable_state.level().tmp_fixed_offset_idx;
        
        const uint16_t fixed_offset_idx_begin_bak = state.get_fixed_offset_idx();

        const auto* type = fixed_variant_type->first_variant();
        for (uint16_t i = 0; i < variant_count; i++) {
            const auto& type_meta = fixed_variant_type->type_metas()[i];


            const auto field_counts = (type_meta.level_fixed_leafs
                + lexer::LeafCounts::of(type_meta.level_fixed_variants + type_meta.level_fixed_arrays)).counts();
            const auto field_count_total = field_counts.total();

            const uint16_t queued_fields_start = queued_fields_base;

            FixedVariantLevel::MutableState::Level level_mutable_state {
                lexer::LeafSizes::zero(),
                lexer::LeafCounts::Counts::zero(),
                queued_fields_start,
                tmp_fixed_offset_idx_base
            };

            console.debug("variant fixed leafs: ", type_meta.level_fixed_leafs.counts());
            console.debug("variant fields: ", field_counts);
            console.debug("Queued position: ", level_mutable_state.queue_position , " total: ", field_count_total);

            type = type->visit(TypeVisitor<lexer::Type, FixedVariantLevel::State, in_array, in_fixed_size>{
                FixedVariantLevel::State{
                    FixedVariantLevel::ConstState{
                        state.const_state.shared(),
                        FixedVariantLevel::ConstState::Level{
                            queued_fields_buffer,
                            fixed_offset_idx_begin_bak,
                            state.const_state.level().get_pack_info_base_idx(),
                        },
                    },
                    FixedVariantLevel::MutableState{
                        state.mutable_state.shared(),
                        level_mutable_state
                    }
                }
            }).next_type;

            tmp_fixed_offset_idx_base = level_mutable_state.tmp_fixed_offset_idx;

            console.debug("Queued position after: ", level_mutable_state.queue_position);
            queued_fields_base = level_mutable_state.queue_position;
            
            const uint64_t used_space = level_mutable_state.used_spaces.total();

            variant_leaf_metas[i] = {
                level_mutable_state.used_spaces,
                used_space,
                level_mutable_state.non_zero_fields_counts,
                {queued_fields_start, queued_fields_base}
            };
            
            max_used_space = std::max(used_space, max_used_space);
        }

        std::sort(variant_leaf_metas, variant_leaf_metas + variant_count, [](const VariantLeafMeta& a, const VariantLeafMeta& b) {
            return a.required_space > b.required_space;
        });
        console.debug("max_used_space: ", max_used_space);
        BSSERT(variant_leaf_metas[0].required_space == max_used_space, "Sorting of variants' leaf metadata invalid")
          
        // try perferect layout
        const variant_layout::Layout layout = variant_layout::perfect::find_st(variant_leaf_metas, queued_fields_buffer.data(), variant_count);
        console.debug("layout: ", layout);
        if (layout.align1 != max_used_space) {
            console.warn("Could not find perfect layout for variant.");
        }

        BSSERT(state.get_fixed_offset_idx() == fixed_offset_idx_begin_bak); // Inside variants no fixed_offsets should be added directy.

        apply_layout<lexer::SIZE::SIZE_8>(
            queued_fields_buffer,
            layout,
            variant_count,
            variant_leaf_metas,
            state.get_fixed_offset_idx(),
            {}
        );
        }
    }

    void on_packed_variant (const lexer::PackedVariantType* const  /*unused*/) const {
        INTERNAL_ERROR("Packed variant not supported yet");
    }

    void on_dynamic_variant (const lexer::DynamicVariantType* const /*unused*/) const {
        INTERNAL_ERROR("Dynamic variant not supported yet");
    }

    void on_identifier (const lexer::IdentifiedType* const identified_type) const {
        const auto* const identifier = state.const_state.shared().ast_buffer.get(identified_type->identifier_idx);
        if (identifier->keyword != lexer::KEYWORDS::STRUCT) {
            INTERNAL_ERROR("expected struct");
        }
        const auto* const struct_type = identifier->data()->as_struct();

        struct_type->visit([&](const lexer::StructField::Data* field_data) {
            return field_data->type()->visit(this->as_visitor_for<lexer::StructField>()).next_type;
        });
    }

    template<typename NewNextType>
    [[nodiscard]] constexpr const TypeVisitor<NewNextType, State, in_array, in_fixed_size>& as_visitor_for() const {
        return estd::sibling_cast<const TypeVisitor<NewNextType, State, in_array, in_fixed_size>&>(*this);
    }
};

struct GenerateResult {
    Buffer var_offset_buffer;
    uint64_t var_leafs_start;
};

GenerateResult generate (
    const lexer::StructDefinition* target_struct,
    const ReadOnlyBuffer& ast_buffer,
    const std::span<FixedOffset> fixed_offsets,
    const std::span<Buffer::View<uint64_t>> var_offsets,
    const std::span<uint16_t> idx_map,
    const std::span<ArrayPackInfo> pack_infos,
    Buffer&& var_offset_buffer,
    const lexer::LeafCounts& level_fixed_leafs,
    const lexer::LeafCounts::Counts& var_leaf_counts,
    const uint16_t& total_var_leafs,
    const uint16_t& level_fixed_variants,
    const uint16_t& level_fixed_arrays,
    const uint16_t& /*unused*/
) {    
    // uint64_t var_leaf_sizes[total_var_leafs];
    console.debug("total var leafs: ", total_var_leafs);
    ALLOCA_SAFE_SPAN(var_leaf_sizes, uint64_t, total_var_leafs);
    std::ranges::uninitialized_fill(var_leaf_sizes, static_cast<uint64_t>(-1));
    ALLOCA_SAFE_SPAN(size_leafe_idxs, uint16_t, total_var_leafs);
    std::ranges::uninitialized_fill(size_leafe_idxs, static_cast<uint16_t>(-1));

    console.debug("level_fixed_leafs: ", level_fixed_leafs.counts());
    console.debug("level_fixed_variants: ", level_fixed_variants);
    console.debug("level_fixed_arrays: ", level_fixed_arrays);

    TopLevel::MutableState::Data top_level_mutable_state_data {
        TopLevel::MutableState::Shared{
            std::move(var_offset_buffer)
        },
        TopLevel::MutableState::Level{
            (level_fixed_leafs + lexer::LeafCounts::of(level_fixed_variants + level_fixed_arrays)).counts(),
            create_positions(var_leaf_counts)
        }
    };

    ALLOCA_UNSAFE_SPAN(tmp_fixed_offsets, FixedOffset, fixed_offsets.size());
    std::ranges::uninitialized_fill(tmp_fixed_offsets, FixedOffset::empty());

    TypeVisitor<lexer::StructField, TopLevel::State, false, true> top_level_visitor {
        TopLevel::State{
            TopLevel::ConstState{
                TopLevel::ConstState::Shared{
                    ast_buffer,
                    fixed_offsets,
                    tmp_fixed_offsets,
                    var_offsets,
                    idx_map,
                    pack_infos
                },
                TopLevel::ConstState::Level{
                    var_leaf_sizes,
                    size_leafe_idxs
                }
            },
            TopLevel::MutableState{
                top_level_mutable_state_data
            }
        }
    };

    console.debug("TopLevel:: ... left_fields: ", top_level_visitor.state.mutable_state.level().left_fields);

    target_struct->visit([&top_level_visitor](const lexer::StructField::Data* field_data) {
        return field_data->type()->visit(top_level_visitor).next_type;
    });

    uint64_t offset = top_level_mutable_state_data.level.current_offset;

    console.debug("queued size: ", top_level_mutable_state_data.level.queued.fields.size());

    if (total_var_leafs > 0) {
        uint8_t max_var_leaf_align;
        if (var_leaf_counts.align8 > 0) {
            max_var_leaf_align = 8;
        } else if (var_leaf_counts.align4 > 0) {
            max_var_leaf_align = 4;
        } else if (var_leaf_counts.align2 > 0) {
            max_var_leaf_align = 2;
        } else {
            goto done;
        }
        {
            // Add padding based on alignment
            const size_t mod = offset % max_var_leaf_align;
            const size_t padding = (max_var_leaf_align - mod) & (max_var_leaf_align - 1);
            offset += padding;
        }
        done:;
    }

    // visitor_state.set_var_offsets(total_var_leafs, level_size_leafs_count, 0);

    return {
        std::move(top_level_mutable_state_data.shared.var_offset_buffer),
        offset
    };
}


}