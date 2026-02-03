#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gsl/pointers>
#include <gsl/util>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "../parser/lexer_types.hpp"
#include "../container/memory.hpp"
#include "../util/logger.hpp"
#include "../helper/internal_error.hpp"
#include "../helper/alloca.hpp"
#include "./variant_layout/perfect_st.hpp"
#include "../estd/utility.hpp"
#include "../common_data.hpp"
#include "./layout_data.hpp"
#include "./variant_layout/variant_layout.hpp"
#include "./tvs.hpp"


namespace layout_generation {

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
        ALLOCA_UNSAFE_SPAN(variant_leaf_metas, VariantLeafMeta, variant_count);
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

        std::ranges::sort(variant_leaf_metas, [](const VariantLeafMeta& a, const VariantLeafMeta& b) {
            return a.required_space > b.required_space;
        });
        console.debug("max_used_space: ", max_used_space);
        BSSERT(variant_leaf_metas[0].required_space == max_used_space, "Sorting of variants' leaf metadata invalid")
          
        // try perferect layout
        const variant_layout::Layout layout = variant_layout::perfect::find_st(variant_leaf_metas, queued_fields_buffer);
        console.debug("layout: ", layout);
        if (layout.align1 != max_used_space) {
            console.warn("Could not find perfect layout for variant.");
        }

        BSSERT(state.get_fixed_offset_idx() == fixed_offset_idx_begin_bak); // Inside variants no fixed_offsets should be added directy.

        state.template next_variant_packs<lexer::SIZE::SIZE_8>(
            variant_layout::apply_layout<lexer::SIZE::SIZE_8>(
                queued_fields_buffer,
                state.const_state.shared().fixed_offsets,
                state.const_state.shared().tmp_fixed_offsets,
                layout,
                variant_leaf_metas,
                state.get_fixed_offset_idx(),
                {}
            )
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
        offset = lexer::next_multiple(offset, var_leaf_counts.largest_align());
    }

    // visitor_state.set_var_offsets(total_var_leafs, level_size_leafs_count, 0);

    return {
        std::move(top_level_mutable_state_data.shared.var_offset_buffer),
        offset
    };
}


}