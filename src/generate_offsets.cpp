#pragma once

#include "base.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gsl/pointers>
#include <gsl/util>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#include "./parser/lexer_types.hpp"
#include "./estd/ranges.hpp"
#include "./container/memory.hpp"
#include "./util/logger.hpp"
#include "./helper/internal_error.hpp"
#include "./variant_optimizer/perfect_st.hpp"
#include "helper/ce.hpp"
#include "subset_sum_solving/subset_sum_perfect.hpp"
#include "util/string_literal.hpp"
#include "util/sys_info.hpp"
#include "variant_optimizer/data.hpp"


namespace generate_offsets {

template <lexer::SIZE size>
[[nodiscard]] constexpr estd::integral_range<uint16_t> get_range (const lexer::LeafCounts::Counts counts) {
    uint16_t start;
    uint16_t end;
    if constexpr (size == lexer::SIZE::SIZE_8) {
        start = 0;
        end = counts.align8;
    } else if constexpr (size == lexer::SIZE::SIZE_4) {
        start = counts.align8;
        end = start + counts.align4;
    } else if constexpr (size == lexer::SIZE::SIZE_2) {
        start = counts.align8 + counts.align4;
        end = start + counts.align2;
    } else {
        start = counts.align8 + counts.align4 + counts.align2;
        end = start + counts.align1;
    }
    return {start, end};
}

template <
    lexer::SIZE alignment,
    bool aligned,
    bool has_variants
>
[[nodiscard]] inline uint64_t _set_sizes (
    uint64_t offset,
    const lexer::LeafCounts::Counts& fixed_leaf_starts,
    const lexer::LeafCounts::Counts& fixed_leaf_ends,
    const uint16_t& level_fixed_variants,
    FixedOffset* const& fixed_offsets,
    const VariantField* const& variant_fields_buffer
) {
    if constexpr (aligned) {
        for (uint16_t i = fixed_leaf_starts.get<alignment>(); i < fixed_leaf_ends.get<alignment>(); i++) {
            FixedOffset& local_offset = fixed_offsets[i];
            const uint64_t size = local_offset.get_offset();
            local_offset.set_offset(offset);
            //logger::debug("i: ", i, ", offset: ", offset, ", size: ", size);
            offset += size;
        }
        if constexpr (has_variants) {
            uint16_t variant_field_i = 0;
            const uint16_t variant_fields_last = level_fixed_variants - 1;
            while (true) {
                const auto& variant_field = variant_fields_buffer[variant_field_i];
                const VariantField::Range& variant_field_range = lexer::get_align_member<alignment>(variant_field);
                const uint64_t size = variant_field.sizes.get<alignment>();
                logger::debug("variant_field_i: ", variant_field_i, ", offset: ", offset, ", size: ", size);
                for (uint16_t& i : variant_field_range) {
                    fixed_offsets[i].increment_offset(offset);
                    logger::debug("adding variant field i: ", i, ", offset: ", offset, ", size: ", size);
                }
                offset += size;

                if (variant_field_i == variant_fields_last) {
                    break;
                }
                variant_field_i++;
            }
            if constexpr (alignment == lexer::SIZE::SIZE_1) {
                return offset;
            } else {
                return _set_sizes<alignment.next_smaller(), true, has_variants>(offset, fixed_leaf_starts, fixed_leaf_ends, level_fixed_variants, fixed_offsets, variant_fields_buffer);
            }
        } else {
            if constexpr (alignment == lexer::SIZE::SIZE_1) {
                return offset;
            } else {
                return _set_sizes<alignment.next_smaller(), true, has_variants>(offset, fixed_leaf_starts, fixed_leaf_ends, level_fixed_variants, fixed_offsets, variant_fields_buffer);
            }
        }
    } else {
        static_assert(false, "must be aligned");
    }
}

[[nodiscard]] inline uint64_t set_sizes (
    uint64_t offset,
    const lexer::LeafCounts::Counts& fixed_leaf_starts,
    const lexer::LeafCounts::Counts& fixed_leaf_ends,
    const uint16_t& level_fixed_variants,
    FixedOffset* const& fixed_offsets,
    const VariantField* const& variant_fields_buffer
) {
    if (level_fixed_variants > 0) {
        return _set_sizes<lexer::SIZE::SIZE_8, true, true>(offset, fixed_leaf_starts, fixed_leaf_ends, level_fixed_variants, fixed_offsets, variant_fields_buffer);
    } else {
        return _set_sizes<lexer::SIZE::SIZE_8, true, false>(offset, fixed_leaf_starts, fixed_leaf_ends, level_fixed_variants, fixed_offsets, variant_fields_buffer);
    }
}

void print_leaf_counts (const std::string_view& name, const lexer::LeafCounts::Counts& counts) {
    logger::debug(name, ": {8:", counts.align1, ", 16:", counts.align2, ", 32:", counts.align4, ", 64:", counts.align8, ", total:", counts.total(), "}");
}

struct TypeVisitorState {

    struct ConstState {
        constexpr ConstState (
            const ReadOnlyBuffer& ast_buffer,
            FixedOffset* const& fixed_offsets,
            Buffer::View<uint64_t>* const& var_offsets,
            uint16_t* const& idx_map
        ) :
        ast_buffer(ast_buffer),
        fixed_offsets(fixed_offsets),
        var_offsets(var_offsets),
        idx_map(idx_map)
        {}

        ReadOnlyBuffer ast_buffer;              // Buffer containing the AST
        FixedOffset* fixed_offsets;             // Represets the size of each fixed size leaf. Once leafs are arranged it represents the offset
        Buffer::View<uint64_t>* var_offsets;    // Represents the size of the variable size leaf. Used for genrating the offset calc strings
        uint16_t* idx_map;                      // Maps occurence in the AST to a stored leaf
    };

    struct LevelConstState {
        constexpr LevelConstState (
            uint64_t* const& var_leaf_sizes,
            VariantField* const& variant_fields,
            uint16_t* const& size_leafe_idxs,
            const uint16_t& var_idx_base,
            FixedLeaf* fixed_leafs
        ) :
        var_leaf_sizes(var_leaf_sizes),
        variant_fields(variant_fields),
        size_leafe_idxs(size_leafe_idxs),
        fixed_leafs(fixed_leafs),
        var_idx_base(var_idx_base)
        {}
        uint64_t* var_leaf_sizes;       // Stores the size of each variable size leaf
        VariantField* variant_fields;   // Stores info about where the leafes of the variant are located
        uint16_t* size_leafe_idxs;      // Since varirable sized leafs also are sorted by alignment we need this mapping to their insertion order
        FixedLeaf* fixed_leafs;         // Only used in variants
        uint16_t var_idx_base;
    };

    struct MutableState {
        constexpr MutableState (
            Buffer&& var_offset_buffer,
            const uint16_t& current_map_idx,
            const uint16_t& current_fixed_idx_base,
            const uint16_t& current_var_idx_base
        ) :
        var_offset_buffer(std::move(var_offset_buffer)),
        current_map_idx(current_map_idx),
        current_fixed_idx_base(current_fixed_idx_base),
        current_var_idx_base(current_var_idx_base)
        {}

        MutableState (const MutableState& other) = delete;
        MutableState (MutableState&& other) = delete;
        

        Buffer var_offset_buffer;           // Stores the size chains for variable size leaf offsets
        uint16_t current_map_idx;           // The current index into ConstState::idx_map
        uint16_t current_fixed_idx_base;    // The current base index for fixed sized leafs (maybe can be moved into LevelConstState if we know the total fixed leaf count including nested levels)
        uint16_t current_var_idx_base;      // The current base index for variable sized leafs
    };

    struct LevelMutableState {
        constexpr LevelMutableState (
            const lexer::LeafCounts::Counts& fixed_leaf_positions,
            const lexer::LeafCounts::Counts& var_leaf_positions,
            const uint16_t& current_variant_field_idx
        ) :
        fixed_leaf_positions(fixed_leaf_positions),
        var_leaf_positions(var_leaf_positions),
        current_variant_field_idx(current_variant_field_idx),
        current_size_leaf_idx(0)
        {}

        LevelMutableState (const LevelMutableState& other) = delete;
        LevelMutableState (LevelMutableState&& other) = delete;

        lexer::LeafCounts::Counts fixed_leaf_positions;
        lexer::LeafCounts::Counts var_leaf_positions;
        uint16_t current_variant_field_idx;
        uint16_t current_size_leaf_idx;
    };

    constexpr TypeVisitorState (
        const ConstState& const_state,
        const LevelConstState& level_const_state,
        const gsl::not_null<MutableState*>& mutable_state,
        const gsl::not_null<LevelMutableState*>& level_mutable_state
    ) :
    const_state(const_state),
    level_const_state(level_const_state),
    mutable_state(mutable_state),
    level_mutable_state(level_mutable_state)
    {}

    constexpr TypeVisitorState(const TypeVisitorState& other) = default;
    constexpr TypeVisitorState(TypeVisitorState&& other) = default;

    ConstState const_state;
    LevelConstState level_const_state;
    gsl::not_null<MutableState*> mutable_state;
    gsl::not_null<LevelMutableState*> level_mutable_state;

    [[nodiscard]] uint16_t next_map_idx () const {
        return mutable_state->current_map_idx++;
    }

    template <bool is_fixed>
    [[nodiscard]] lexer::LeafCounts::Counts& get_positions () const {
        if constexpr (is_fixed) {
            return level_mutable_state->fixed_leaf_positions;
        } else {
            return level_mutable_state->var_leaf_positions;
        }
    }


    template <bool is_fixed, bool in_variant, lexer::SIZE alignment>
    [[nodiscard]] uint16_t next_idx () const {
        lexer::LeafCounts::Counts& positions = get_positions<is_fixed>();
        const uint16_t idx = positions.get<alignment>()++;
        if constexpr (!is_fixed) {
            auto v = level_mutable_state->current_size_leaf_idx;
            logger::debug("size_leafe_idxs[", idx, "] = ", v);
            level_const_state.size_leafe_idxs[idx] = v;
        }
        return idx;
    }

    template <bool is_fixed, bool in_variant>
    [[nodiscard]] uint16_t next_idx (lexer::SIZE alignment) const {
        switch (alignment) {
            case lexer::SIZE::SIZE_1:
                return next_idx<is_fixed, in_variant, lexer::SIZE::SIZE_1>();
            case lexer::SIZE::SIZE_2:
                return next_idx<is_fixed, in_variant, lexer::SIZE::SIZE_2>();
            case lexer::SIZE::SIZE_4:
                return next_idx<is_fixed, in_variant, lexer::SIZE::SIZE_4>();
            case lexer::SIZE::SIZE_8:
                return next_idx<is_fixed, in_variant, lexer::SIZE::SIZE_8>();
            default:
                INTERNAL_ERROR("[next_idx] invalid size");
        }
    }
    
    template <bool is_fixed, bool in_variant>
    void reserve (uint16_t idx, uint64_t size, lexer::SIZE alignment) const {
        if constexpr (is_fixed) {
            logger::debug("setting fixed_offsets at: ", idx, ", size: ", size);
            const uint16_t map_idx = next_map_idx();
            if constexpr (in_variant) {
                BSSERT(level_const_state.fixed_leafs != nullptr)
                level_const_state.fixed_leafs[idx] = {size, map_idx};
            } else {
                const_state.fixed_offsets[idx] = {size, alignment};
            }
            logger::debug("map_idx: ", map_idx, ", idx: ", idx, ", size: ", size);
            const_state.idx_map[map_idx] = idx;
        } else {
            if constexpr (in_variant) {
                BSSERT(false, "EPIC FAIL")
            }
            logger::debug("setting var_leaf_sizes at: ", idx, ", size: ", size, " in_variant: ", in_variant, " ptr: ", size_t(level_const_state.var_leaf_sizes + idx));
            level_const_state.var_leaf_sizes[idx] = size;
            const uint16_t map_idx = next_map_idx();
            const uint16_t mapped_idx = idx + level_const_state.var_idx_base;
            logger::debug("map_idx: ", map_idx, ", idx: ", idx, ", mapped_idx: ", mapped_idx, ", size: ", size);
            const_state.idx_map[map_idx] = mapped_idx;
        }
    }
    
    template <bool is_fixed, bool in_variant, lexer::SIZE alignment>
    void reserve_next (uint64_t size) const {
        reserve<is_fixed, in_variant>(
            next_idx<is_fixed, in_variant, alignment>(),
            size,
            alignment
        );
    }

    template <bool is_fixed, bool in_variant>
    void reserve_next (lexer::SIZE alignment, uint64_t size) const {
        reserve<is_fixed, in_variant>(
            next_idx<is_fixed, in_variant>(alignment),
            size,
            alignment
        );
    }


    [[nodiscard]] VariantField& next_variant_field () const {
        return level_const_state.variant_fields[level_mutable_state->current_variant_field_idx++];
    }

    void set_var_offsets (
        const uint16_t& total_var_leafs,
        const uint16_t& level_size_leafs_count,
        const uint16_t& var_leaf_base_idx
    ) const {
        uint64_t size_leafs[level_size_leafs_count];
        const auto* const& var_leaf_sizes = level_const_state.var_leaf_sizes;
        const auto& size_leafe_idxs = level_const_state.size_leafe_idxs;
        auto& var_offset_buffer = mutable_state->var_offset_buffer;
        logger::debug("level_size_leafs_count: ", level_size_leafs_count);
        for (uint16_t i = 0; i < total_var_leafs; i++) {
            std::uninitialized_fill_n(size_leafs, level_size_leafs_count, 0);
            logger::debug("Var leaf: i: ", i, ", size: ", var_leaf_sizes[i], " ptr: ", size_t(var_leaf_sizes + i));
            uint16_t known_size_leafs = 0;
            for (uint16_t j = 0; j < i; j++) {
                const uint16_t size_leaf_idx = size_leafe_idxs[j];
                uint64_t& size_leaf = size_leafs[size_leaf_idx];
                if (size_leaf == 0) {
                    known_size_leafs++;
                }
                logger::debug("loop 1: Size leaf #", size_leaf_idx, ": size: ", size_leaf, " added:", var_leaf_sizes[j]);
                size_leaf += var_leaf_sizes[j];
            }
            logger::log<true, true, logger::debug_prefix>("Dump var_leaf_sizes(): {");
            for (uint16_t j = 0; j < total_var_leafs - 1; j++) {
                logger::log<true, true>(var_leaf_sizes[j], ", ");
            }
            logger::log(var_leaf_sizes[total_var_leafs - 1], "}");
            logger::debug("Size chain: Length: ", known_size_leafs);
            // BSSERT(known_size_leafs <= level_size_leafs_count, "Too many size leafs");
            const Buffer::Index<uint64_t> size_chain_idx = var_offset_buffer.next_multi_byte<uint64_t>(sizeof(uint64_t) * known_size_leafs);
            uint64_t* const size_chain_data = var_offset_buffer.get_aligned(size_chain_idx);
            for (uint16_t size_leaf_idx = 0; size_leaf_idx < known_size_leafs; size_leaf_idx++) {
                const uint64_t size = size_leafs[size_leaf_idx];
                logger::debug("Size leaf #", size_leaf_idx, ": size: ", size);
                size_chain_data[size_leaf_idx] = size;
            }
            logger::debug("Writing var_offset: ", var_leaf_base_idx + i);
            const_state.var_offsets[var_leaf_base_idx + i] = Buffer::View<uint64_t>{size_chain_idx, known_size_leafs};
        }
    }
};

template <lexer::SIZE size>
requires (size != lexer::SIZE::SIZE_0)
constexpr void set_ez_perfect_layout (VariantField& variant_field, VariantField::Range range) {
    if constexpr (size == lexer::SIZE::SIZE_8) {
        variant_field.align8 = range;
    } else {
        variant_field.align8 = VariantField::Range::empty();
    }
    if constexpr (size == lexer::SIZE::SIZE_4) {
        variant_field.align4 = range;
    } else {
        variant_field.align4 = VariantField::Range::empty();
    }
    if constexpr (size == lexer::SIZE::SIZE_2) {
        variant_field.align2 = range;
    } else {
        variant_field.align2 = VariantField::Range::empty();
    }
    if constexpr (size == lexer::SIZE::SIZE_1) {
        variant_field.align1 = range;
    } else {
        variant_field.align1 = VariantField::Range::empty();
    }
}

template <lexer::SIZE alignment>
[[nodiscard]] inline uint16_t apply_layout (
    FixedLeaf* const fixed_leafs_buffer,
    VariantField* const variant_fields_buffer,
    VariantField& variant_field,
    const lexer::LeafSizes layout,
    const uint16_t variant_count,
    FixedOffset* const fixed_offsets,
    uint16_t* const idx_map,
    VariantLeafMeta* const variant_leaf_metas,
    uint16_t ordered_idx,
    lexer::LeafSizes& pacK_sizes,
    uint64_t outer_array_length
) {
    logger::debug("[apply_layout] alignemnt: ", alignment.byte_size());
    VariantField::Range& leafs_range = lexer::get_align_member<alignment>(variant_field);
    leafs_range.from = ordered_idx;
    uint64_t max_offset = 0;
    for (uint16_t i = 0; i < variant_count; i++) {
        uint64_t offset = 0;
        auto& meta = variant_leaf_metas[i];
        
        uint64_t required = 0;

        for (VariantField& variant_field : std::ranges::subrange{
            variant_fields_buffer + meta.variant_field_idxs.from,
            variant_fields_buffer + meta.variant_field_idxs.to
        }) {
            uint64_t& size = variant_field.sizes.get<alignment>();
            if constexpr (alignment != lexer::SIZE::SIZE_8) {
                if (size == 0) continue;
            }
            const VariantField::Range& leafs_range = lexer::get_align_member<alignment>(variant_field);
            for (FixedOffset& fixed_offset : std::ranges::subrange{
                fixed_offsets + leafs_range.from,
                fixed_offsets + leafs_range.to
            }) {
                fixed_offset.increment_offset(offset);
            }
            offset += size;
            required += size;
            size = 0;
        }
        
        const FixedLeaf* it_start;
        if constexpr (alignment == lexer::SIZE::SIZE_8) {
            it_start = fixed_leafs_buffer + meta.fixed_leafs_start;
        } else {
            it_start = fixed_leafs_buffer + meta.fixed_leafs_ends.get<alignment.next_bigger()>();
        }
        const uint16_t copied_end = meta.fixed_leafs_ends.get<alignment>();
        const FixedLeaf* const it_end = fixed_leafs_buffer + copied_end;
        // logger::debug("[apply_layout] alignemnt: ", alignment.byte_size(), " i: ", i, " start: ", size_t(it_start), " end: ", size_t(it_end), " copied: ", meta.fixed_leaf_ends.align4);
        for (const auto* it = it_start; it != it_end; it++) {
            const FixedLeaf leaf = *it;
            if constexpr (alignment != lexer::SIZE::SIZE_8) {
                if (leaf.is_zero()) continue;
            }
            uint64_t leaf_size = leaf.get_size();
            uint16_t map_idx = leaf.get_map_idx();
            idx_map[map_idx] = ordered_idx;
            fixed_offsets[ordered_idx] = {offset, alignment};
            ordered_idx++;
            offset += leaf_size;
            required += leaf_size;
        }
        // logger::debug("[apply_layout] alignemnt: ", alignment.byte_size(), " i: ", i);
        uint64_t layout_space;
        if constexpr (alignment == lexer::SIZE::SIZE_8) {
            layout_space = layout.align8;
        } else if constexpr (alignment == lexer::SIZE::SIZE_4) {
            layout_space = layout.align4 - layout.align8;
        } else if constexpr (alignment == lexer::SIZE::SIZE_2) {
            layout_space = layout.align2 - layout.align4;
        } else if constexpr (alignment == lexer::SIZE::SIZE_1) {
            layout_space = layout.align1 - layout.align2;
        } else {
            static_assert(false);
        }
        BSSERT(layout_space >= required, "The layout doesn't fulfill space requirements for "_sl + string_literal::from<alignment.byte_size()> + " byte aligned section of Variant "_sl, layout_space, " >= ", required);
        
        if constexpr (alignment != lexer::SIZE::SIZE_1) {
            // if (copied_end == meta.fixed_leafs_ends.align1) goto skip;

            const auto used_space = meta.used_spaces.get<alignment>();
            if (layout_space > used_space) {
                logger::debug("required_space: ", meta.required_space, ", layout_space: ", layout_space, ", used_space: ", used_space, ", required: ", required);
                uint64_t target = std::min<uint64_t>(layout_space - used_space, meta.required_space);
                std::tie(ordered_idx, offset) = subset_sum_perfect::solve<alignment>(
                    target,
                    fixed_leafs_buffer,
                    meta,
                    copied_end,
                    fixed_offsets,
                    meta.variant_field_idxs,
                    variant_fields_buffer,
                    idx_map,
                    ordered_idx,
                    offset
                );
            }
        }

        // skip:
        meta.required_space -= offset;
        max_offset = std::max(offset, max_offset);
    }
    leafs_range.to = ordered_idx;
    pacK_sizes.get<alignment>() = max_offset;
    variant_field.sizes.get<alignment>() = max_offset * outer_array_length;
    return ordered_idx;
}

[[nodiscard]] constexpr lexer::LeafCounts::Counts create_positions (const lexer::LeafCounts::Counts& counts, uint16_t offset = 0) {
    return {
        gsl::narrow_cast<uint16_t>(offset + counts.align8 + counts.align4 + counts.align2),
        gsl::narrow_cast<uint16_t>(offset + counts.align8 + counts.align4),
        gsl::narrow_cast<uint16_t>(offset + counts.align8),
        offset
    };
}

void print_leaf_sizes (std::string_view name, lexer::LeafSizes sizes) {
    logger::debug(name, ": {8: ", sizes.align8, ", 4: ", sizes.align4, ", 2: ", sizes.align2, ", 1: ", sizes.align1, "}");
}


template <typename TypeT, bool is_fixed, bool in_array, bool in_variant>
struct TypeVisitor : public lexer::TypeVisitorBase<TypeT> {

    constexpr TypeVisitor (
        const lexer::Type* const& type,        
        const uint64_t& outer_array_length,
        const TypeVisitorState::ConstState& const_state,
        const TypeVisitorState::LevelConstState& level_const_state,
        const gsl::not_null<TypeVisitorState::MutableState*>& mutable_state,
        const gsl::not_null<TypeVisitorState::LevelMutableState*>& level_mutable_state
    ) :
    lexer::TypeVisitorBase<TypeT>{type},
    state(const_state, level_const_state, mutable_state, level_mutable_state),
    outer_array_length(outer_array_length)
    {}

    constexpr TypeVisitor (
        const lexer::Type* const& type,        
        const uint64_t& outer_array_length,
        TypeVisitorState  state
    ) :
    lexer::TypeVisitorBase<TypeT>{type},
    state(std::move(state)),
    outer_array_length(outer_array_length)
    {}

    TypeVisitorState state;
    uint64_t outer_array_length;

    template <lexer::FIELD_TYPE field_type>
    void on_simple () const {
        logger::debug("[on_simple] field_type: ", (uint8_t)field_type, " outer_array_length: ", outer_array_length, " is_fixed: ", is_fixed, " in_variant: ", in_variant);
        constexpr lexer::SIZE size = lexer::get_type_alignment<field_type>();
        state.reserve_next<is_fixed, in_variant, size>(size.byte_size() * outer_array_length);
    }

    void on_bool     (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::BOOL   >(); }
    void on_uint8    (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::UINT8  >(); }
    void on_uint16   (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::UINT16 >(); }
    void on_uint32   (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::UINT32 >(); }
    void on_uint64   (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::UINT64 >(); }
    void on_int8     (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::INT8   >(); }
    void on_int16    (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::INT16  >(); }
    void on_int32    (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::INT32  >(); }
    void on_int64    (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::INT64  >(); }
    void on_float32  (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::FLOAT32>(); }
    void on_float64  (estd::empty /*unused*/) const override { on_simple<lexer::FIELD_TYPE::FLOAT64>(); }

    void on_fixed_string (estd::empty /*unused*/, const lexer::FixedStringType* const fixed_string_type) const override {
        const uint32_t length = fixed_string_type->length;
        state.reserve_next<is_fixed, in_variant, lexer::SIZE::SIZE_1>(outer_array_length * length);
    }

    void on_string (estd::empty /*unused*/, const lexer::StringType* const string_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Variable length strings in arrays not supported");
        } else {
            const lexer::SIZE stored_size_size = string_type->stored_size_size;
            state.reserve_next<false, in_variant, lexer::SIZE::SIZE_1>(1);

            state.reserve_next<true, in_variant>(stored_size_size, stored_size_size.byte_size());
        }
    }

    [[nodiscard]] TypeVisitor::ResultT on_fixed_array (estd::empty /*unused*/, const lexer::ArrayType* const fixed_array_type) const override {
        return TypeVisitor<TypeT, is_fixed, true, in_variant>{
            fixed_array_type->inner_type(),
            fixed_array_type->length * outer_array_length,
            state
        }.visit();
    }


    [[nodiscard]] TypeVisitor::ResultT on_array (estd::empty /*unused*/, const lexer::ArrayType* const array_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
            const lexer::SIZE stored_size_size = array_type->stored_size_size;

            typename TypeVisitor::ResultT next_type = TypeVisitor<TypeT, false, true, in_variant>{
                array_type->inner_type(),
                1,
                state
            }.visit();

            state.reserve_next<true, in_variant>(stored_size_size, stored_size_size.byte_size());

            state.level_mutable_state->current_size_leaf_idx++;

            return next_type;
        }
    }

    [[nodiscard]] TypeVisitor::ResultT on_fixed_variant (estd::empty /*unused*/, lexer::FixedVariantType* const fixed_variant_type) const override {

        if constexpr (in_array && !is_fixed) {
            INTERNAL_ERROR("Fixed variants in variabl sized arrays not supported yet");
        }

        const uint16_t variant_count = fixed_variant_type->variant_count;

        logger::debug("[variant_id] outer_array_length: ", outer_array_length);
        if (variant_count <= UINT8_MAX) {
            state.reserve_next<is_fixed, in_variant, lexer::SIZE::SIZE_1>(outer_array_length);
        } else {
            state.reserve_next<is_fixed, in_variant, lexer::SIZE::SIZE_2>(outer_array_length);
        }

        uint16_t fixed_variants_total = 0;
        lexer::LeafCounts fixed_leaf_counts_total = lexer::LeafCounts::zero();
        for (uint16_t i = 0; i < variant_count; i++) {
            const lexer::FixedVariantTypeMeta& meta = fixed_variant_type->type_metas()[i];
            fixed_variants_total += meta.level_fixed_variants;
            fixed_leaf_counts_total += meta.fixed_leaf_counts;
        }
        const uint16_t total_fixed_leafs = fixed_leaf_counts_total.total();

        const uint16_t level_fixed_idx_start = state.mutable_state->current_fixed_idx_base;
        const uint16_t level_fixed_idx_end = level_fixed_idx_start + total_fixed_leafs;
        state.mutable_state->current_fixed_idx_base = level_fixed_idx_end;
        logger::debug("level fixed_idx range: ", level_fixed_idx_start, " - ", level_fixed_idx_end);

        FixedLeaf fixed_leafs_buffer[total_fixed_leafs];
        uint16_t fixed_leafs_buffer_base = 0;

        uint64_t max_used_space = 0;
        VariantLeafMeta variant_leaf_metas[variant_count];

        VariantField variant_fields_buffer[fixed_variants_total];
        uint16_t variant_field_buffer_base = 0;

        VariantField& variant_field = state.next_variant_field();

        const auto* type = fixed_variant_type->first_variant();
        for (uint16_t i = 0; i < variant_count; i++) {
            const auto& type_meta = fixed_variant_type->type_metas()[i];
            const auto fixed_leaf_counts = type_meta.fixed_leaf_counts.counts;
            const auto total_fixed_leaf_count = fixed_leaf_counts.total();
            const auto fixed_variant_count = type_meta.level_fixed_variants;

            const lexer::LeafCounts::Counts fixed_leaf_starts = create_positions(fixed_leaf_counts, fixed_leafs_buffer_base);

            TypeVisitorState::LevelMutableState level_mutable_state {
                fixed_leaf_starts,
                lexer::LeafCounts::Counts::zero(),
                0
            };

            const uint16_t variant_fields_start = variant_field_buffer_base;
            variant_field_buffer_base += fixed_variant_count;
            const uint16_t variant_fields_end = variant_field_buffer_base;

            type = TypeVisitor<lexer::Type, is_fixed, in_array, true>{
                type,
                1,
                state.const_state,
                TypeVisitorState::LevelConstState{
                    nullptr,
                    variant_fields_buffer + variant_fields_start,
                    nullptr,
                    0,
                    fixed_leafs_buffer
                },
                state.mutable_state,
                &level_mutable_state
            }.visit().next_type;

            

            const lexer::LeafCounts::Counts& fixed_leafs_ends = level_mutable_state.fixed_leaf_positions; // this was advanced in TypeVisitor so now its the end
            BSSERT(fixed_leafs_ends.align8 == fixed_leafs_buffer_base + fixed_leaf_counts.align8);
            BSSERT(fixed_leafs_ends.align4 == fixed_leafs_buffer_base + fixed_leaf_counts.align8 + fixed_leaf_counts.align4);
            BSSERT(fixed_leafs_ends.align2 == fixed_leafs_buffer_base + fixed_leaf_counts.align8 + fixed_leaf_counts.align4 + fixed_leaf_counts.align2);
            BSSERT(fixed_leafs_ends.align1 == fixed_leafs_buffer_base + fixed_leaf_counts.total());

            lexer::LeafSizes used_spaces = lexer::LeafSizes::zero();
            FixedLeaf* current = fixed_leafs_buffer + fixed_leafs_buffer_base;
            FixedLeaf* current_end = current + fixed_leaf_counts.align8;
            BSSERT(current_end - current == fixed_leaf_counts.align8);
            for (; current != current_end; current++) { used_spaces.align8 += current->get_size(); }
            current_end += fixed_leaf_counts.align4;
            BSSERT(current_end - current == fixed_leaf_counts.align4);
            for (; current != current_end; current++) { used_spaces.align4 += current->get_size(); }
            current_end += fixed_leaf_counts.align2;
            BSSERT(current_end - current == fixed_leaf_counts.align2);
            for (; current != current_end; current++) { used_spaces.align2 += current->get_size(); }
            current_end += fixed_leaf_counts.align1;
            BSSERT(current_end - current == fixed_leaf_counts.align1);
            for (; current != current_end; current++) { used_spaces.align1 += current->get_size(); }
            for (const VariantField& variant_field : std::ranges::subrange{
                variant_fields_buffer + variant_fields_start,
                variant_fields_buffer + variant_fields_end
            }) {
                used_spaces += variant_field.sizes;
            }
            auto used_space = used_spaces.total();
            variant_leaf_metas[i] = {
                fixed_leafs_ends,
                fixed_leafs_buffer_base,
                {variant_fields_start, variant_fields_end},
                used_space,
                used_spaces
            };
            max_used_space = std::max(used_space, max_used_space);
            
            fixed_leafs_buffer_base += total_fixed_leaf_count;
            BSSERT(fixed_leafs_buffer + fixed_leafs_buffer_base == current);
        }

        #if 0 // Disabled since unfinished
        if (max_used_space % fixed_variant_type->max_alignment.byte_size() == 0) {
            auto layout = lexer::LeafSizes::from_space_at_size(fixed_variant_type->max_alignment, max_used_space);
            print_leaf_sizes("ez perfect layout", layout);
            VariantField::Range range {{level_fixed_idx_start, level_fixed_idx_end}, max_used_space};
            #define CASE(SIZE)                                       \
            case SIZE: {                                             \
                set_ez_perfect_layout<SIZE>(variant_field, range);   \
                break;                                               \
            }
            switch (fixed_variant_type->max_alignment) {
                CASE(lexer::SIZE::SIZE_1);
                CASE(lexer::SIZE::SIZE_2);
                CASE(lexer::SIZE::SIZE_4);
                CASE(lexer::SIZE::SIZE_8);
                default:
                    BSSERT(false, "unreachable reached")
                    std::unreachable();
                
            }
            #undef CASE
            return typename TypeVisitor::ResultT{reinterpret_cast<TypeVisitor::ConstTypeT*>(type)};
        }
        #endif

        std::sort(variant_leaf_metas, variant_leaf_metas + variant_count, [](const VariantLeafMeta& a, const VariantLeafMeta& b) {
            return a.required_space > b.required_space;
        });
        logger::debug(max_used_space, " ", variant_leaf_metas[0].required_space);
        BSSERT(variant_leaf_metas[0].required_space == max_used_space, "Sorting of variants' leaf metadata invalid")
          
        // try perferect layout
        const auto layout = find_perfect_variant_layout_st(variant_leaf_metas, fixed_leafs_buffer, variant_fields_buffer, variant_count);
        print_leaf_sizes("layout", layout);
        if (layout.align1 != max_used_space) {
            logger::warn("Could not find perfect layout for variant.");
        }
        uint16_t ordered_idx = level_fixed_idx_start;

        if (layout.align8 == 0) goto non_perfect_at_align8;
        ordered_idx = apply_layout<lexer::SIZE::SIZE_8>(fixed_leafs_buffer, variant_fields_buffer, variant_field, layout, variant_count, state.const_state.fixed_offsets, state.const_state.idx_map, variant_leaf_metas, ordered_idx, fixed_variant_type->pack_sizes, outer_array_length);
        if (layout.align4 == 0) goto non_perfect_at_align4;
        ordered_idx = apply_layout<lexer::SIZE::SIZE_4>(fixed_leafs_buffer, variant_fields_buffer, variant_field, layout, variant_count, state.const_state.fixed_offsets, state.const_state.idx_map, variant_leaf_metas, ordered_idx, fixed_variant_type->pack_sizes, outer_array_length);
        if (layout.align2 == 0) goto non_perfect_at_align2;
        ordered_idx = apply_layout<lexer::SIZE::SIZE_2>(fixed_leafs_buffer, variant_fields_buffer, variant_field, layout, variant_count, state.const_state.fixed_offsets, state.const_state.idx_map, variant_leaf_metas, ordered_idx, fixed_variant_type->pack_sizes, outer_array_length);
        BSSERT(layout.align1 != 0, "This should not happen. layout for alignemnt 1 is always defined as long as higher alignments are defined");
        ordered_idx = apply_layout<lexer::SIZE::SIZE_1>(fixed_leafs_buffer, variant_fields_buffer, variant_field, layout, variant_count, state.const_state.fixed_offsets, state.const_state.idx_map, variant_leaf_metas, ordered_idx, fixed_variant_type->pack_sizes, outer_array_length);
        goto fixed_variant_done;

        non_perfect_at_align8:
        variant_field.align8 = VariantField::Range::empty();
        non_perfect_at_align4:
        variant_field.align4 = VariantField::Range::empty();
        non_perfect_at_align2:
        variant_field.align2 = VariantField::Range::empty();
        variant_field.align1 = VariantField::Range::empty();

        BSSERT(ordered_idx == level_fixed_idx_end);
        
        logger::debug(
            "current_fixed_idx_base: ", state.mutable_state->current_fixed_idx_base,
            ", start_variant_idx: ", level_fixed_idx_start,
            ", end_variant_idx: ", level_fixed_idx_end
        );

        fixed_variant_done:
        return typename TypeVisitor::ResultT{reinterpret_cast<TypeVisitor::ConstTypeT*>(type)};
    }

    [[nodiscard]] TypeVisitor::ResultT on_packed_variant (estd::empty /*unused*/, const lexer::PackedVariantType* const  /*unused*/) const override {
        INTERNAL_ERROR("Packed variant not supported yet");
    }

    [[nodiscard]] TypeVisitor::ResultT on_dynamic_variant (estd::empty /*unused*/, const lexer::DynamicVariantType* const /*unused*/) const override {
        INTERNAL_ERROR("Dynamic variant not supported yet");
        /* if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic variant in array not supported");
        } else {
            const uint16_t variant_count = dynamic_variant_type->variant_count;
            logger::debug("variant_count: ", variant_count);
            logger::debug("max_alignment: ", dynamic_variant_type->max_alignment);
            logger::debug("stored_size_size: ", dynamic_variant_type->stored_size_size, ", lexer::SIZE:SIZE_1: ", lexer::SIZE::SIZE_1);
            logger::debug("size_size: ", dynamic_variant_type->size_size);
            logger::debug("level_fixed_leafs: ", dynamic_variant_type->level_fixed_leaf_counts.total());
            logger::debug("level_var_leafs: ", dynamic_variant_type->level_total_var_leafs);

            logger::debug("[variant_id] outer_array_length: ", outer_array_length);
            if (variant_count <= UINT8_MAX) {
                state.reserve_next<is_fixed, in_variant, lexer::SIZE::SIZE_1>(outer_array_length);
            } else {
                state.reserve_next<is_fixed, in_variant, lexer::SIZE::SIZE_2>(outer_array_length * 2);
            }

            const lexer::SIZE stored_size_size = dynamic_variant_type->stored_size_size;
            const uint16_t leaf_idx = state.next_idx<true, in_variant>(stored_size_size);
            logger::debug("VARIANT size leaf_idx: ", leaf_idx);
            logger::debug("DYNAMIC_VARIANT stored_size_size: ", stored_size_size);
            state.const_state.fixed_offsets[leaf_idx] = stored_size_size.byte_size();

            const uint16_t map_idx = state.next_map_idx();
            state.const_state.idx_map[map_idx] = leaf_idx;

            state.reserve_next<false, in_variant>(dynamic_variant_type->max_alignment, 1);

            const auto* type = dynamic_variant_type->first_variant();

            const lexer::LeafCounts::Counts level_fixed_leaf_counts = dynamic_variant_type->level_fixed_leaf_counts.counts;

            const uint16_t level_fixed_idx_start = state.mutable_state->current_fixed_idx_base;
            const uint16_t level_fixed_idx_end = level_fixed_idx_start + level_fixed_leaf_counts.total();
            state.mutable_state->current_fixed_idx_base = level_fixed_idx_end;

            const uint16_t level_var_idx_start = state.mutable_state->current_var_idx_base;
            const uint16_t level_var_idx_end = level_var_idx_start + dynamic_variant_type->level_total_var_leafs;
            state.mutable_state->current_var_idx_base = level_var_idx_end;

            uint16_t level_current_var_idx_base = level_var_idx_start;

            lexer::LeafCounts::Counts level_fixed_leaf_positions = create_positions(level_fixed_leaf_counts, level_fixed_idx_start);

            logger::debug("level fixed_idx range: [", level_fixed_idx_start, ", ", level_fixed_idx_end, ")");
            logger::debug("level var_idx range: [", level_var_idx_start, ", ", level_var_idx_end, ")");
            uint64_t max_offset = 0;
            uint64_t min_offset = UINT64_MAX;
            
            for (uint16_t i = 0; i < variant_count; i++) {
                const auto& type_meta = dynamic_variant_type->type_metas()[i];
                const auto fixed_leaf_counts = type_meta.fixed_leaf_counts.counts;
                const auto var_leaf_counts = type_meta.var_leaf_counts.counts;
                const uint16_t level_fixed_variants = type_meta.level_fixed_variants;
                const auto level_size_leafs_count = type_meta.level_size_leafs;
                const auto total_fixed_leafs = fixed_leaf_counts.total();
                const auto total_var_leafs = var_leaf_counts.total();

                uint64_t var_leaf_sizes[total_var_leafs];
                uint16_t size_leafe_idxs[level_size_leafs_count];
                VariantField variant_fields[level_fixed_variants];


                TypeVisitorState::LevelMutableState level_mutable_state {
                    level_fixed_leaf_positions,
                    create_positions(var_leaf_counts),
                    0
                };

                type = TypeVisitor<const lexer::Type, true, in_array, true>{
                    type,
                    1,
                    state.const_state,
                    TypeVisitorState::LevelConstState{
                        var_leaf_sizes,
                        variant_fields,
                        size_leafe_idxs,
                        level_current_var_idx_base
                    },
                    state.mutable_state,
                    &level_mutable_state
                }.visit().next_type;

                uint64_t offset = 0;

                const lexer::LeafCounts::Counts& fixed_leaf_ends = level_mutable_state.fixed_leaf_positions;

                offset = set_sizes(
                    offset,
                    level_fixed_leaf_positions,
                    fixed_leaf_ends,
                    level_fixed_variants,
                    state.const_state.fixed_offsets,
                    variant_fields
                );

                state.set_var_offsets(total_var_leafs, level_size_leafs_count, level_current_var_idx_base);

                max_offset = std::max(max_offset, offset);
                min_offset = std::min(min_offset, offset);

                level_current_var_idx_base += total_var_leafs;
                level_fixed_leaf_positions = fixed_leaf_ends;
            }

            if constexpr (in_array) {
                max_offset *= outer_array_length;
            }

            logger::debug(
                "current_fixed_idx_base: ", state.mutable_state->current_fixed_idx_base,
                ", max_offset: ", max_offset,
                ", start_variant_idx: ", level_fixed_idx_start,
                ", end_variant_idx: ", level_fixed_idx_end
            );
            
            return typename TypeVisitor::ResultT(reinterpret_cast<TypeVisitor::ConstTypeT*>(type));
        } */
    }

    void on_identifier (estd::empty /*unused*/, const lexer::IdentifiedType* const identified_type) const override {
        const auto* const identifier = state.const_state.ast_buffer.get(identified_type->identifier_idx);
        if (identifier->keyword != lexer::KEYWORDS::STRUCT) {
            INTERNAL_ERROR("expected struct");
        }
        const auto* const struct_type = identifier->data()->as_struct();

        const auto* field = struct_type->first_field();
        for (uint16_t i = 0; i < struct_type->field_count; i++) {
            const auto* const field_data = field->data();

            field = TypeVisitor<lexer::StructField, is_fixed, in_array, in_variant>{
                field_data->type(),
                outer_array_length,
                state
            }.visit().next_type;
        }
    }
};

struct GenerateResult {
    Buffer var_offset_buffer;
    uint64_t var_leafs_start;
};

GenerateResult generate (
    const lexer::StructDefinition* target_struct,
    const TypeVisitorState::ConstState& const_state,
    Buffer&& var_offset_buffer,
    const lexer::LeafCounts::Counts& fixed_leaf_counts,
    const lexer::LeafCounts::Counts& var_leaf_counts,
    const uint16_t& total_fixed_leafs,
    const uint16_t& total_var_leafs,
    const uint16_t& level_fixed_variants,
    const uint16_t& level_size_leafs_count
) {
    const uint16_t total_variant_fixed_leafs = target_struct->total_variant_fixed_leafs;
    
    uint64_t var_leaf_sizes[total_var_leafs];
    logger::debug("total var leafs: ", total_var_leafs);
    uint16_t size_leafe_idxs[total_var_leafs];
    VariantField variant_fields_buffer[level_fixed_variants];

    TypeVisitorState::MutableState mutable_state {
        std::move(var_offset_buffer),
        0,
        total_fixed_leafs,
        total_var_leafs
    };

    const lexer::LeafCounts::Counts fixed_leaf_starts = create_positions(fixed_leaf_counts);

    TypeVisitorState::LevelMutableState level_mutable_state {
        fixed_leaf_starts,
        create_positions(var_leaf_counts),
        0
    };

    TypeVisitorState visitor_state {
        const_state,
        TypeVisitorState::LevelConstState{
            var_leaf_sizes,
            variant_fields_buffer,
            size_leafe_idxs,
            0,
            nullptr
        },
        &mutable_state,
        &level_mutable_state
    };

    const auto* field = target_struct->first_field();
    for (uint16_t i = 0; i < target_struct->field_count; i++) {
        const auto *const field_data = field->data();

        field = TypeVisitor<lexer::StructField, true, false, false>{
            field_data->type(),
            1,
            visitor_state
        }.visit().next_type;
    }

    for (uint16_t i = 0; i < total_fixed_leafs; i++) {
        logger::debug("Fixed leaf: i: ", i, ", size: ", const_state.fixed_offsets[i].get_offset());
    }
    for (uint16_t i = total_fixed_leafs; i < total_fixed_leafs + total_variant_fixed_leafs; i++) {
        logger::debug("Variant leaf: i: ", i, ", size: ", const_state.fixed_offsets[i].get_offset());
    }

    uint64_t offset = 0;

    offset = set_sizes(
        offset,
        fixed_leaf_starts,
        level_mutable_state.fixed_leaf_positions,
        level_fixed_variants,
        const_state.fixed_offsets,
        variant_fields_buffer
    );

    for (uint16_t i = 0; i < total_fixed_leafs; i++) {
        logger::debug("Fixed leaf: i: ", i, ", offset: ", const_state.fixed_offsets[i].get_offset());
    }
    for (uint16_t i = total_fixed_leafs; i < total_fixed_leafs + total_variant_fixed_leafs; i++) {
        logger::debug("Variant leaf: i: ", i, ", offset: ", const_state.fixed_offsets[i].get_offset());
    }



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

    visitor_state.set_var_offsets(total_var_leafs, level_size_leafs_count, 0);

    return {
        std::move(mutable_state.var_offset_buffer),
        offset
    };
}


}