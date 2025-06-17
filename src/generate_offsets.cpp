#pragma once
#include "base.cpp"
#include "fatal_error.cpp"
#include "memory.cpp"
#include <algorithm>
#include <cstdint>
#include <utility>
#include "lexer_types.cpp"
#include "logger.cpp"

namespace generate_offsets {

struct Offsets {
    Offsets (
        uint64_t* const fixed_offsets,
        Buffer::View<uint64_t>* const var_offsets,
        uint64_t* const variant_offsets,
        uint16_t* const idx_map
    ) :
    fixed_offsets(fixed_offsets),
    var_offsets(var_offsets),
    variant_offsets(variant_offsets),
    idx_map(idx_map)
    {}
    uint64_t* const fixed_offsets;
    Buffer::View<uint64_t>* const var_offsets;
    uint64_t* const variant_offsets;
    uint16_t* const idx_map;
};

struct SizeLeaf {
    uint16_t leaf_idx;
};

struct VariantField {
    uint64_t size;
    uint32_t start;
    uint32_t end;
};

template <lexer::SIZE size, std::integral T>
INLINE constexpr T next_multiple (T value) {
    constexpr T mask = (static_cast<T>(1) << static_cast<uint8_t>(size)) - 1;
    T result =  (value + mask) & ~mask;
    return result;
}
template <typename F, typename Ret, typename... Args>
concept CallableWith = requires(F func, Args... args) {
    { func(args...) } -> std::same_as<Ret>;
};

template <std::integral T>
struct Range {
    T start;
    T end;
};

template <lexer::SIZE size>
[[clang::always_inline]] inline Range<uint16_t> get_range (const lexer::LeafCounts::Counts counts) {
    uint16_t start;
    uint16_t end;
    if constexpr (size == lexer::SIZE::SIZE_8) {
        start = 0;
        end = counts.size64;
    } else if constexpr (size == lexer::SIZE::SIZE_4) {
        start = counts.size64;
        end = start + counts.size32;
    } else if constexpr (size == lexer::SIZE::SIZE_2) {
        start = counts.size64 + counts.size32;
        end = start + counts.size16;
    } else {
        start = counts.size64 + counts.size32 + counts.size16;
        end = start + counts.size8;
    }
    return {start, end};
}

template <lexer::SIZE size>
[[clang::always_inline]] inline const uint16_t& get_count (const lexer::LeafCounts::Counts& counts) {
    if constexpr (size == lexer::SIZE::SIZE_8) {
        return counts.size64;
    } else if constexpr (size == lexer::SIZE::SIZE_4) {
        return counts.size32;
    } else if constexpr (size == lexer::SIZE::SIZE_2) {
        return counts.size16;
    } else {
        return counts.size8;
    }
}

template <lexer::SIZE size>
requires (size != lexer::SIZE::SIZE_0)
consteval lexer::SIZE next_smaller_size () {
    if constexpr (size == lexer::SIZE::SIZE_8) {
        return lexer::SIZE::SIZE_4;
    } else if constexpr (size == lexer::SIZE::SIZE_4) {
        return lexer::SIZE::SIZE_2;
    } else if constexpr (size == lexer::SIZE::SIZE_2) {
        return lexer::SIZE::SIZE_1;
    } else if constexpr (size == lexer::SIZE::SIZE_1) {
        return lexer::SIZE::SIZE_0;
    }
}

template <lexer::SIZE size = lexer::SIZE::SIZE_8, bool aligned = true, std::invocable_r<uint64_t, uint16_t, uint64_t> F_set_offset, std::invocable_r<uint64_t, uint16_t, uint64_t> F_add_variant_base>
[[clang::always_inline]] inline uint64_t set_sizes (uint64_t offset, const lexer::LeafCounts::Counts fixed_leaf_counts, const lexer::LeafCounts::Counts variant_field_counts, F_set_offset&& set_offset, F_add_variant_base&& add_variant_base) {
    if constexpr (size == lexer::SIZE::SIZE_0) {
        return offset;
    } else {
        if constexpr (aligned) {
            auto [fixed_leafs_start, fixed_leafs_end] = get_range<size>(fixed_leaf_counts);
            for (uint16_t i = fixed_leafs_start; i < fixed_leafs_end; i++) {
                offset = set_offset(i, offset);
            }
            if (get_count<size>(variant_field_counts) > 0) {
                auto [variant_field_start, variant_field_end] = get_range<size>(variant_field_counts);
                offset = add_variant_base(variant_field_start, offset);
                for (uint16_t i = variant_field_start + 1; i < variant_field_end; i++) {
                    offset = next_multiple<size>(offset);
                    offset = add_variant_base(i, offset);
                }
                return set_sizes<next_smaller_size<size>(), false>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
            } else {
                return set_sizes<next_smaller_size<size>(), true>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
            }
        } else {
            if (get_count<size>(fixed_leaf_counts) > 0) {
                auto [fixed_leafs_start, fixed_leafs_end] = get_range<size>(fixed_leaf_counts);
                offset = next_multiple<size>(offset);
                offset = set_offset(fixed_leafs_start, offset);
                for (uint16_t i = fixed_leafs_start + 1; i < fixed_leafs_end; i++) {
                    offset = set_offset(i, offset);
                }
                if (get_count<size>(variant_field_counts) > 0) {
                    auto [variant_field_start, variant_field_end] = get_range<size>(variant_field_counts);
                    offset = add_variant_base(variant_field_start, offset);
                    for (uint16_t i = variant_field_start + 1; i < variant_field_end; i++) {
                        offset = next_multiple<size>(offset);
                        offset = add_variant_base(i, offset);
                    }
                    return set_sizes<next_smaller_size<size>(), false>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
                } else {
                    return set_sizes<next_smaller_size<size>(), true>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
                }
            } else {
                auto [variant_field_start, variant_field_end] = get_range<size>(variant_field_counts);
                for (uint16_t i = variant_field_start; i < variant_field_end; i++) {
                    offset = next_multiple<size>(offset);
                    offset = add_variant_base(i, offset);
                }
                return set_sizes<next_smaller_size<size>(), false>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
            }
        }
    }
}

// template <bool in_variant>
// INLINE void set_size_leafs (
//     Leafs<in_variant> leafs
// ) {
//     for (uint16_t i = 0; i < leafs.total_size_leafs; i++) {
//         auto [leaf_idx] = leafs.size_leafs()[i];
//         uint16_t map_idx = leafs.leaf_sizes.current_map_idx_ptr[0]++;
//         leafs.leaf_sizes.idx_map[map_idx] = leaf_idx;
//         LOG("size_leaf map_idx: %d, leaf_idx: %d\n", 0, leaf_idx);
//     }
// }


struct TypeVisitorState {

    struct ConstState {
        constexpr ConstState (
            const Buffer& buffer,
            uint64_t* fixed_offsets,
            uint64_t*  variant_offsets,
            Buffer::View<uint64_t>* var_offsets,
            uint16_t* idx_map
        ) :
        buffer(estd::const_copy(buffer)),
        fixed_offsets(fixed_offsets),
        variant_offsets(variant_offsets),
        var_offsets(var_offsets),
        idx_map(idx_map)
        {}

        ConstState (const ConstState& other) : buffer(estd::const_copy(other.buffer)), fixed_offsets(other.fixed_offsets), variant_offsets(other.variant_offsets), var_offsets(other.var_offsets), idx_map(other.idx_map) {}

        const Buffer buffer;
        uint64_t* fixed_offsets;                  // represets the size of each fixed size leaf. once leafs are arranged it represents the offset
        // uint64_t* const var;                         // represents the size of the variable size leaf. used for genrating the offset calc strings
        uint64_t* variant_offsets;                //
        Buffer::View<uint64_t>* var_offsets;      //
        uint16_t* idx_map;                        // maps occurence in the AST to a stored leaf
    
    };

    struct LevelConstState {
        constexpr LevelConstState (
            uint64_t* const var_leaf_sizes,
            VariantField* const variant_fields,
            uint16_t* const size_leafe_idxs,
            const lexer::LeafCounts::Counts fixed_leaf_counts,
            const lexer::LeafCounts::Counts var_leaf_counts,
            const lexer::LeafCounts::Counts variant_field_counts,
            const uint16_t fixed_idx_base,
            const uint16_t var_idx_base
        ) :
        var_leaf_sizes(var_leaf_sizes),
        variant_fields(variant_fields),
        size_leafe_idxs(size_leafe_idxs),
        fixed_leaf_counts(fixed_leaf_counts),
        var_leaf_counts(var_leaf_counts),
        variant_field_counts(variant_field_counts),
        fixed_idx_base(fixed_idx_base),
        var_idx_base(var_idx_base)
        {}

        //explicit LevelConstState (const LevelConstState& other) = delete;
        //explicit LevelConstState (LevelConstState&& other) = delete;

        uint64_t* const var_leaf_sizes;
        VariantField* const variant_fields;
        uint16_t* const size_leafe_idxs;
        const lexer::LeafCounts::Counts fixed_leaf_counts;
        const lexer::LeafCounts::Counts var_leaf_counts;
        const lexer::LeafCounts::Counts variant_field_counts;
        const uint16_t fixed_idx_base;
        const uint16_t var_idx_base;
    };

    struct MutableState {
        constexpr MutableState (
            Buffer&& var_offset_buffer,
            uint16_t current_map_idx,
            uint16_t current_variant_field_idx,
            uint16_t current_fixed_idx_base,
            uint16_t current_var_idx_base
        ) :
        var_offset_buffer(std::move(var_offset_buffer)),
        current_map_idx(current_map_idx),
        current_variant_field_idx(current_variant_field_idx),
        current_fixed_idx_base(current_fixed_idx_base),
        current_var_idx_base(current_var_idx_base)
        {}

        explicit MutableState (const MutableState& other) = delete;
        explicit MutableState (MutableState&& other) = delete;
        

        Buffer var_offset_buffer;
        uint16_t current_map_idx;
        uint16_t current_variant_field_idx;
        uint16_t current_fixed_idx_base;
        uint16_t current_var_idx_base;
    };

    struct LevelMutableState {
        constexpr LevelMutableState (
            lexer::LeafCounts::Counts fixed_leaf_positions,
            lexer::LeafCounts::Counts var_leaf_positions,
            lexer::LeafCounts::Counts variant_field_positions,
            uint16_t current_fixed_idx_base,
            uint16_t current_var_idx_base,
            uint16_t current_size_leaf_idx
        ) :
        fixed_leaf_positions(fixed_leaf_positions),
        var_leaf_positions(var_leaf_positions),
        variant_field_positions(variant_field_positions),
        current_fixed_idx_base(current_fixed_idx_base),
        current_var_idx_base(current_var_idx_base),
        current_size_leaf_idx(current_size_leaf_idx)
        {}

        explicit LevelMutableState (const LevelMutableState& other) noexcept = delete;
        explicit LevelMutableState (LevelMutableState&& other) noexcept = delete;

        lexer::LeafCounts::Counts fixed_leaf_positions;
        lexer::LeafCounts::Counts var_leaf_positions;
        lexer::LeafCounts::Counts variant_field_positions;
        uint16_t current_fixed_idx_base;
        uint16_t current_var_idx_base;
        uint16_t current_size_leaf_idx;
    };

    constexpr TypeVisitorState (
        const ConstState& const_state,
        const LevelConstState& level_const_state,
        MutableState& mutable_state,
        LevelMutableState& level_mutable_state
    ) :
    const_state(const_state),
    level_const_state(level_const_state),
    mutable_state(mutable_state),
    level_mutable_state(level_mutable_state)
    {}

    constexpr TypeVisitorState (
        ConstState&& const_state,
        LevelConstState&& level_const_state,
        MutableState& mutable_state,
        LevelMutableState& level_mutable_state
    ) :
    const_state(std::move(const_state)),
    level_const_state(std::move(level_const_state)),
    mutable_state(mutable_state),
    level_mutable_state(level_mutable_state)
    {}

    constexpr TypeVisitorState(const TypeVisitorState& other) = default;
    constexpr TypeVisitorState(TypeVisitorState&& other) = default;

    const ConstState const_state;
    const LevelConstState level_const_state;
    MutableState& mutable_state;
    LevelMutableState& level_mutable_state;

    uint16_t next_map_idx () const {
        return mutable_state.current_map_idx++;
    }

    void set_next_map_idx_offset (uint64_t offset) const {
        const_state.fixed_offsets[next_map_idx()] = offset;
    }

    uint16_t next_variant_field_idx () const {
        return mutable_state.current_variant_field_idx++;
    }

    void set_next_variant_offset (uint64_t offset) const {
        const_state.variant_offsets[next_variant_field_idx()] = offset;
    }

    template <bool is_fixed>
    const lexer::LeafCounts::Counts& get_counts () const {
        if constexpr (is_fixed) {
            return level_const_state.fixed_leaf_counts;
        } else {
            return level_const_state.var_leaf_counts;
        }
    }

    template <bool is_fixed>
    lexer::LeafCounts::Counts& get_positions () const {
        if constexpr (is_fixed) {
            return level_mutable_state.fixed_leaf_positions;
        } else {
            return level_mutable_state.var_leaf_positions;
        }
    }


    template <bool is_fixed, bool in_variant, lexer::SIZE alignment>
    INLINE uint16_t next_idx () const {
        const lexer::LeafCounts::Counts& counts = get_counts<is_fixed>();
        lexer::LeafCounts::Counts& positions = get_positions<is_fixed>();
        if (get_count<alignment>(positions) >= get_count<alignment>(counts)) {
            logger::error("[next_idx] position overflow is_fixed: ", is_fixed ? "true" : "false", ", in_variant: ", in_variant ? "true" : "false", ", alignment: ", alignment.byte_size() );
            exit(1);
        }
        uint16_t base_idx;
        if constexpr (alignment == lexer::SIZE::SIZE_1) {
            base_idx = counts.size64 + counts.size32 + counts.size16 + positions.size8++;
        } else if constexpr (alignment == lexer::SIZE::SIZE_2) {
            base_idx = counts.size64 + counts.size32 + positions.size16++;
        } else if constexpr (alignment == lexer::SIZE::SIZE_4) {
            base_idx = counts.size64 + positions.size32++;
        } else if constexpr (alignment == lexer::SIZE::SIZE_8) {
            base_idx = positions.size64++;
        } else {
            static_assert(false, "Invalid size");
        }
        if constexpr (is_fixed) {
            if constexpr (in_variant) {
                level_mutable_state.current_fixed_idx_base++;
                logger::debug("base_idx: ", base_idx, ", fixed_idx_base: ", level_const_state.fixed_idx_base);
            }
            return base_idx + level_const_state.fixed_idx_base;
        } else {
            if constexpr (in_variant) {
                level_mutable_state.current_var_idx_base++;
                logger::debug("base_idx: ", base_idx, ", var_idx_base: ", level_const_state.var_idx_base);
            }
            level_const_state.size_leafe_idxs[base_idx] = level_mutable_state.current_size_leaf_idx++;
            return base_idx;
        }
    }

    template <bool is_fixed, bool in_variant>
    INLINE uint16_t next_idx (lexer::SIZE alignment) const {
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
    
    template <bool is_fixed>
    INLINE void reserve (uint16_t idx, uint64_t size) const {
        if constexpr (is_fixed) {
            const_state.fixed_offsets[idx] = size;
            uint16_t map_idx = next_map_idx();
            logger::debug("map_idx: ", map_idx, ", idx: ", idx, ", size: ", size);
            const_state.idx_map[map_idx] = idx;
        } else {
            level_const_state.var_leaf_sizes[idx] = size;
            uint16_t map_idx = next_map_idx();
            uint16_t mapped_idx = idx + level_const_state.var_idx_base;
            logger::debug("map_idx: ", map_idx, ", idx: ", idx, ", mapped_idx: ", mapped_idx, ", size: ", size);
            const_state.idx_map[map_idx] = mapped_idx;
        }
    }
    
    template <bool is_fixed, bool in_variant, lexer::SIZE alignment>
    INLINE void reserve_next (uint64_t size) const {
        reserve<is_fixed>(
            next_idx<is_fixed, in_variant, alignment>(),
            size
        );
    }

    template <bool is_fixed, bool in_variant>
    INLINE void reserve_next (lexer::SIZE alignment, uint64_t size) const {
        reserve<is_fixed>(
            next_idx<is_fixed, in_variant>(alignment),
            size
        );
    }

    INLINE VariantField* next_variant_field (lexer::SIZE alignment) const {
        uint16_t idx;
        auto& variant_field_counts = level_const_state.variant_field_counts;
        auto& variant_field_positions = level_mutable_state.variant_field_positions;
        switch (alignment) {
        case lexer::SIZE::SIZE_1:
            idx = variant_field_counts.size64 + variant_field_counts.size32 + variant_field_counts.size16 + variant_field_positions.size8++;
            break;
        case lexer::SIZE::SIZE_2:
            idx = variant_field_counts.size64 + variant_field_counts.size32 + variant_field_positions.size16++;
            break;
        case lexer::SIZE::SIZE_4:
            idx = variant_field_counts.size64 + variant_field_positions.size32++;
            break;
        case lexer::SIZE::SIZE_8:
            idx = variant_field_positions.size64++;
            break;
        case lexer::SIZE::SIZE_0:
            INTERNAL_ERROR("[reserve_variant_field] invalid alignment SIZE_0");
        }
        return level_const_state.variant_fields + idx;
    }

    INLINE void set_var_offsets (
        uint16_t total_var_leafs,
        uint16_t level_size_leafs_count
    ) const {
        uint64_t size_leafs[level_size_leafs_count];
        memset(size_leafs, 0, level_size_leafs_count * sizeof(uint64_t));
        auto& var_leaf_sizes = level_const_state.var_leaf_sizes;
        auto& size_leafe_idxs = level_const_state.size_leafe_idxs;
        auto& var_offset_buffer = mutable_state.var_offset_buffer;
        for (uint16_t i = 0; i < total_var_leafs; i++) {
            logger::debug("Var leaf: cc_idx: ", i, ", size: ", var_leaf_sizes[i]);
            uint16_t known_size_leafs = 0;
            for (uint16_t j = 0; j < i; j++) {
                uint16_t& size_leaf_idx = size_leafe_idxs[j];
                uint64_t* size_leaf = size_leafs + size_leaf_idx;
                if (*size_leaf == 0) {
                    known_size_leafs++;
                }
                logger::debug("loop 1: Size leaf #", size_leaf_idx, ": size: ", *size_leaf);
                *size_leaf += var_leaf_sizes[j];
            }
            logger::debug("Dump var_leaf_sizes(): {", var_leaf_sizes[0]);
            for (uint16_t j = 1; j < total_var_leafs; j++) {
                logger::debug(", ", var_leaf_sizes[j]);
            }
            logger::debug("}");
            logger::debug("Size chain: Length: ", known_size_leafs);
            // BSSERT(known_size_leafs <= level_size_leafs_count, "Too many size leafs");
            const Buffer::Index<uint64_t> size_chain_idx = var_offset_buffer.next_multi_byte<uint64_t>(sizeof(uint64_t) * known_size_leafs);
            uint64_t* const size_chain_data = var_offset_buffer.get_aligned(size_chain_idx);
            for (uint16_t size_leaf_idx = 0; size_leaf_idx < known_size_leafs; size_leaf_idx++) {
                uint64_t size = size_leafs[size_leaf_idx];
                logger::debug("Size leaf #", size_leaf_idx, ": size: ", size);
                size_chain_data[size_leaf_idx] = size;
            }
            logger::debug("Writing var_offset: ", level_mutable_state.current_var_idx_base + i);
            const_state.var_offsets[level_mutable_state.current_var_idx_base + i] = Buffer::View<uint64_t>{size_chain_idx, known_size_leafs};
            memset(size_leafs, 0, sizeof(uint64_t) * known_size_leafs);
            // memset(size_leafs, 0, sizeof(uint64_t) * known_size_leafs);
        }
    }
};

template <typename TypeT, bool is_fixed, bool in_array, bool in_variant>
struct TypeVisitor : public lexer::TypeVisitorBase<TypeT> {

    INLINE constexpr TypeVisitor (
        const lexer::Type* const& type,        
        const uint64_t& outer_array_length,
        const TypeVisitorState::ConstState& const_state,
        const TypeVisitorState::LevelConstState& level_const_state,
        TypeVisitorState::MutableState& mutable_state,
        TypeVisitorState::LevelMutableState& level_mutable_state
    ) :
    lexer::TypeVisitorBase<TypeT>(type),
    state(const_state, level_const_state, mutable_state, level_mutable_state),
    outer_array_length(outer_array_length)
    {}

    INLINE constexpr TypeVisitor (
        const lexer::Type* const& type,        
        const uint64_t& outer_array_length,
        const TypeVisitorState& state
    ) :
    lexer::TypeVisitorBase<TypeT>(type),
    state(state),
    outer_array_length(outer_array_length)
    {}

    const uint64_t outer_array_length;
    const TypeVisitorState state;

    template <lexer::VALUE_FIELD_TYPE field_type>
    INLINE void on_simple () const {
        state.reserve_next<is_fixed, in_variant, lexer::get_type_alignment<field_type>()>(lexer::get_type_size<field_type>() * outer_array_length);
    }

    INLINE void on_bool     () const override { on_simple<lexer::VALUE_FIELD_TYPE::BOOL   >(); }
    INLINE void on_uint8    () const override { on_simple<lexer::VALUE_FIELD_TYPE::UINT8  >(); }
    INLINE void on_uint16   () const override { on_simple<lexer::VALUE_FIELD_TYPE::UINT16 >(); }
    INLINE void on_uint32   () const override { on_simple<lexer::VALUE_FIELD_TYPE::UINT32 >(); }
    INLINE void on_uint64   () const override { on_simple<lexer::VALUE_FIELD_TYPE::UINT64 >(); }
    INLINE void on_int8     () const override { on_simple<lexer::VALUE_FIELD_TYPE::INT8   >(); }
    INLINE void on_int16    () const override { on_simple<lexer::VALUE_FIELD_TYPE::INT16  >(); }
    INLINE void on_int32    () const override { on_simple<lexer::VALUE_FIELD_TYPE::INT32  >(); }
    INLINE void on_int64    () const override { on_simple<lexer::VALUE_FIELD_TYPE::INT64  >(); }
    INLINE void on_float32  () const override { on_simple<lexer::VALUE_FIELD_TYPE::FLOAT32>(); }
    INLINE void on_float64  () const override { on_simple<lexer::VALUE_FIELD_TYPE::FLOAT64>(); }

    INLINE void on_fixed_string (const lexer::FixedStringType* const fixed_string_type) const override {
        auto& length = fixed_string_type->length;
        state.reserve_next<is_fixed, in_variant, lexer::SIZE::SIZE_1>(outer_array_length * length);
    }

    INLINE void on_string (const lexer::StringType* const string_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Variable length strings in arrays not supported");
        } else {
            auto& size_size = string_type->size_size;
            auto& stored_size_size = string_type->stored_size_size;
            state.reserve_next<false, in_variant, lexer::SIZE::SIZE_1>(1);

            uint16_t leaf_idx = state.next_idx<true, in_variant>(stored_size_size);
            logger::debug("STRING.size leaf_idx: ", leaf_idx);
            state.const_state.fixed_offsets[leaf_idx] = stored_size_size.byte_size();

            uint16_t map_idx = state.next_map_idx();
            state.const_state.idx_map[map_idx] = leaf_idx;
        }
    }

    INLINE TypeVisitor::ResultT on_fixed_array (const lexer::ArrayType* const fixed_array_type) const override {
        auto& length = fixed_array_type->length;

        return TypeVisitor<TypeT, is_fixed, true, in_variant>{
            fixed_array_type->inner_type(),
            fixed_array_type->length * outer_array_length,
            state
        }.visit();
    }


    INLINE TypeVisitor::ResultT on_array (const lexer::ArrayType* const array_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
            auto& size_size = array_type->size_size;
            auto& stored_size_size = array_type->stored_size_size;

            typename TypeVisitor::ResultT next_type = TypeVisitor<TypeT, false, true, in_variant>{
                array_type->inner_type(),
                1,
                state
            }.visit();

            uint16_t leaf_idx = state.next_idx<true, in_variant>(stored_size_size);
            state.const_state.fixed_offsets[leaf_idx] = stored_size_size.byte_size();

            uint16_t map_idx = state.next_map_idx();
            state.const_state.idx_map[map_idx] = leaf_idx;

            return next_type;
        }
    }

    INLINE TypeVisitor::ResultT on_fixed_variant (const lexer::FixedVariantType* const fixed_variant_type) const override {
        auto& variant_count = fixed_variant_type->variant_count;

        logger::debug("[variant_id] outer_array_length: ", outer_array_length);
        if (variant_count <= UINT8_MAX) {
            state.reserve_next<is_fixed, in_variant, lexer::SIZE::SIZE_1>(outer_array_length);
        } else {
            state.reserve_next<is_fixed, in_variant, lexer::SIZE::SIZE_2>(outer_array_length);
        }              

        auto type = fixed_variant_type->first_variant();

        const uint16_t level_fixed_idx_start = state.mutable_state.current_fixed_idx_base;
        constexpr uint16_t level_var_idx_start = 0;
        const uint16_t level_fixed_idx_end = level_fixed_idx_start + fixed_variant_type->level_fixed_leafs;
        state.mutable_state.current_fixed_idx_base = level_fixed_idx_end;

        logger::debug("level fixed_idx range: ", level_fixed_idx_start, " - ", level_fixed_idx_end);

        uint16_t variant_field_idx = state.next_variant_field_idx();

        uint64_t max_offset = 0;

        for (uint16_t i = 0; i < variant_count; i++) {
            auto& type_meta = fixed_variant_type->type_metas()[i];
            auto& fixed_leaf_counts = type_meta.fixed_leaf_counts.counts;
            constexpr auto var_leaf_counts = lexer::LeafCounts::zero().counts;
            auto& variant_field_counts = type_meta.variant_field_counts.counts;
            const uint16_t total_fixed_leafs = fixed_leaf_counts.total();
            constexpr uint16_t total_var_leafs = var_leaf_counts.total();
            const uint16_t level_variant_fields = variant_field_counts.total();
            constexpr uint16_t total_size_leafs = 0;
            

            VariantField variant_fields[level_variant_fields];
            
            const TypeVisitorState::LevelConstState level_const_state = {
                nullptr,
                variant_fields,
                nullptr,
                fixed_leaf_counts,
                var_leaf_counts,
                variant_field_counts,
                level_fixed_idx_start,
                level_var_idx_start
            };

            TypeVisitorState::LevelMutableState level_mutable_state = {
                lexer::LeafCounts::zero().counts,
                lexer::LeafCounts::zero().counts,
                lexer::LeafCounts::zero().counts,
                level_fixed_idx_start,
                level_var_idx_start,
                0
            };

            type = TypeVisitor<lexer::Type, is_fixed, in_array, true>{
                type,
                1,
                state.const_state,
                level_const_state,
                state.mutable_state,
                level_mutable_state
            }.visit().next_type;

            uint64_t offset = set_sizes(
                0,
                fixed_leaf_counts,
                variant_field_counts,
                [this, &level_mutable_state](uint16_t i, uint64_t offset) {
                    uint16_t cc_idx = i + level_mutable_state.current_fixed_idx_base;
                    uint64_t size = state.const_state.fixed_offsets[cc_idx];
                    state.const_state.fixed_offsets[cc_idx] = offset;
                    return offset + size;
                },
                [this, &variant_fields](uint16_t i, uint64_t offset) {
                    auto variant_field = variant_fields[i];
                    for (uint16_t cc_idx = variant_field.start; cc_idx < variant_field.end; cc_idx++) {
                        logger::debug("Adding varaint (", i, ") base of ", offset, " to leaf cc_idx: ", cc_idx);
                        state.const_state.fixed_offsets[cc_idx] += offset;
                    }
                    return offset + variant_field.size;
                }
            );
            max_offset = std::max(max_offset, offset);
        }

        uint64_t max_size = in_array ? lexer::next_multiple(max_offset, fixed_variant_type->max_alignment) * outer_array_length : max_offset;

        state.const_state.variant_offsets[variant_field_idx] = max_offset;

        *state.next_variant_field(fixed_variant_type->max_alignment) = VariantField{
            max_size,
            level_fixed_idx_start,
            state.mutable_state.current_fixed_idx_base
        };

        logger::debug("current_fixed_idx_base: ", state.mutable_state.current_fixed_idx_base, ", max_offset: ", max_offset, ", start_variant_idx: ", level_fixed_idx_start, ", level_current_fixed_idx_base: ", state.level_mutable_state.current_fixed_idx_base, ", end_variant_idx: ", level_fixed_idx_end);

        return {reinterpret_cast<TypeVisitor::ConstTypeT*>(type)};
    }

    INLINE TypeVisitor::ResultT on_packed_variant (const lexer::PackedVariantType* const packed_variant_type) const override {
        INTERNAL_ERROR("Packed variant not supported");
    }

    INLINE TypeVisitor::ResultT on_dynamic_variant (const lexer::DynamicVariantType* const dynamic_variant_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic variant in array not supported");
        } else {
            auto& variant_count = dynamic_variant_type->variant_count;
            logger::debug("variant_count: ", variant_count);
            logger::debug("max_alignment: ", dynamic_variant_type->max_alignment);
            logger::debug("stored_size_size: ", dynamic_variant_type->stored_size_size, ", lexer::SIZE:SIZE_1: ", lexer::SIZE::SIZE_1);
            logger::debug("size_size: ", dynamic_variant_type->size_size);
            logger::debug("level_fixed_leafs: ", dynamic_variant_type->level_fixed_leafs);
            logger::debug("level_var_leafs: ", dynamic_variant_type->level_var_leafs);

            logger::debug("[variant_id] outer_array_length: ", outer_array_length);
            if (variant_count <= UINT8_MAX) {
                state.reserve_next<is_fixed, in_variant, lexer::SIZE::SIZE_1>(outer_array_length);
            } else {
                state.reserve_next<is_fixed, in_variant, lexer::SIZE::SIZE_2>(outer_array_length * 2);
            }

            auto& stored_size_size = dynamic_variant_type->stored_size_size;
            uint16_t leaf_idx = state.next_idx<true, in_variant>(stored_size_size);
            logger::debug("VARIANT size leaf_idx: ", leaf_idx);
            logger::debug("DYNAMIC_VARIANT stored_size_size: ", stored_size_size);
            state.const_state.fixed_offsets[leaf_idx] = stored_size_size.byte_size();

            uint16_t map_idx = state.next_map_idx();
            state.const_state.idx_map[map_idx] = leaf_idx;

            state.reserve_next<false, in_variant>(dynamic_variant_type->max_alignment, 1);
            // uint16_t size_leaf_idx = leafs.next_size_leaf_idx();
            // logger::debug("VARIANT size_leaf_idx: ", size_leaf_idx);
            // leafs.size_leafs()[size_leaf_idx] = {variant_type->min_byte_size, leaf_idx, variant_type->size_size, stored_size_size};
            // leafs.size_leafs()[size_leaf_idx] = {leaf_idx};

            auto type = dynamic_variant_type->first_variant();

            const uint16_t level_fixed_idx_start = state.mutable_state.current_fixed_idx_base;
            const uint16_t level_fixed_idx_end = level_fixed_idx_start + dynamic_variant_type->level_fixed_leafs;
            state.mutable_state.current_fixed_idx_base = level_fixed_idx_end;

            const uint16_t level_var_idx_start = state.mutable_state.current_var_idx_base;
            const uint16_t level_var_idx_end = level_var_idx_start + dynamic_variant_type->level_var_leafs;
            state.mutable_state.current_var_idx_base = level_var_idx_end;

            

            logger::debug("level fixed_idx range: [", level_fixed_idx_start, ", ", level_fixed_idx_end, ")");
            logger::debug("level var_idx range: [", level_var_idx_start, ", ", level_var_idx_end, ")");
            uint64_t max_offset = 0;
            uint64_t min_offset = UINT64_MAX;

            uint16_t variant_field_idx = state.next_variant_field_idx();
            
            for (uint16_t i = 0; i < variant_count; i++) {
                auto& type_meta = dynamic_variant_type->type_metas()[i];
                auto& fixed_leaf_counts = type_meta.fixed_leaf_counts.counts;
                auto& var_leaf_counts = type_meta.var_leaf_counts.counts;
                auto& variant_field_counts = type_meta.variant_field_counts.counts;
                auto& level_size_leafs_count = type_meta.level_size_leafs;
                const auto total_fixed_leafs = fixed_leaf_counts.total();
                const auto total_var_leafs = var_leaf_counts.total();
                const auto level_variant_fields = variant_field_counts.total();

                uint64_t var_leaf_sizes[total_var_leafs];
                uint16_t size_leafe_idxs[level_size_leafs_count];
                VariantField variant_fields[level_variant_fields];

                const TypeVisitorState::LevelConstState level_const_state = {
                    var_leaf_sizes,
                    variant_fields,
                    size_leafe_idxs,
                    fixed_leaf_counts,
                    var_leaf_counts,
                    variant_field_counts,
                    level_fixed_idx_start,
                    level_var_idx_start
                };

                TypeVisitorState::LevelMutableState level_mutable_state = {
                    lexer::LeafCounts::zero().counts,
                    lexer::LeafCounts::zero().counts,
                    lexer::LeafCounts::zero().counts,
                    level_fixed_idx_start,
                    level_var_idx_start,
                    0
                };

                type = TypeVisitor<const lexer::Type, true, in_array, true>{
                    type,
                    1,
                    state.const_state,
                    level_const_state,
                    state.mutable_state,
                    level_mutable_state
                }.visit().next_type;
                // set_size_leafs(variant_leafs);

                uint64_t offset = 0;
                offset = set_sizes(
                    offset,
                    fixed_leaf_counts,
                    variant_field_counts,
                    [this, &level_mutable_state](uint16_t i, uint64_t offset) {
                        uint16_t cc_idx = i + level_mutable_state.current_fixed_idx_base;
                        uint64_t size = state.const_state.fixed_offsets[cc_idx];
                        state.const_state.fixed_offsets[cc_idx] = offset;
                        return offset + size;
                    },
                    [this, &variant_fields](uint16_t i, uint64_t offset) {
                        auto variant_field = variant_fields[i];
                        for (uint16_t cc_idx = variant_field.start; cc_idx < variant_field.end; cc_idx++) {
                            logger::debug("Adding varaint (", i, ") base of ", offset, " to leaf cc_idx: ", cc_idx);
                            // state.const_state.fixed_offsets[cc_idx] += offset;
                        }
                        return offset + variant_field.size;
                    }
                );

                state.set_var_offsets(total_var_leafs, level_size_leafs_count);

                max_offset = std::max(max_offset, offset);
                min_offset = std::min(min_offset, offset);
            }

            if constexpr (in_array) {
                max_offset *= outer_array_length;
            }

            state.const_state.variant_offsets[variant_field_idx] = max_offset;

            logger::debug("current_fixed_idx_base: ", state.mutable_state.current_fixed_idx_base, ", max_offset: ", max_offset, ", start_variant_idx: ", level_fixed_idx_start, ", level_current_fixed_idx_base: ", state.level_mutable_state.current_fixed_idx_base, ", end_variant_idx: ", level_fixed_idx_end);
            
            return reinterpret_cast<TypeVisitor::ConstTypeT*>(type);
        }
    }

    INLINE void on_identifier (const lexer::IdentifiedType* const identified_type) const override {
        auto* const identifier = state.const_state.buffer.get(identified_type->identifier_idx);
        if (identifier->keyword != lexer::KEYWORDS::STRUCT) {
            INTERNAL_ERROR("expected struct");
        }
        auto* const struct_type = identifier->data()->as_struct();

        auto field = struct_type->first_field();
        for (uint16_t i = 0; i < struct_type->field_count; i++) {
            auto field_data = field->data();
            uint16_t struct_depth;

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
    TypeVisitorState::ConstState&& const_state,
    Buffer&& var_offset_buffer,
    const lexer::LeafCounts::Counts fixed_leaf_counts,
    const lexer::LeafCounts::Counts var_leaf_counts,
    const lexer::LeafCounts::Counts variant_field_counts,
    const uint16_t total_fixed_leafs,
    const uint16_t total_var_leafs,
    const uint16_t level_variant_fields,
    const uint16_t level_size_leafs_count
) {
    uint16_t total_variant_fixed_leafs = target_struct->total_variant_fixed_leafs;
    uint16_t total_leafs = total_fixed_leafs + total_var_leafs + total_variant_fixed_leafs;
    uint16_t variant_base_idx = total_fixed_leafs;
    

    uint64_t var_leaf_sizes[total_var_leafs];
    logger::debug("total var leafs: ", total_var_leafs);
    uint16_t size_leafe_idxs[level_size_leafs_count];
    VariantField variant_fields[level_variant_fields];

    TypeVisitorState::MutableState mutable_state = {
        std::move(var_offset_buffer),
        0,
        0,
        total_fixed_leafs,
        total_var_leafs
    };

    TypeVisitorState::LevelMutableState level_mutable_state = {
        lexer::LeafCounts::zero().counts,
        lexer::LeafCounts::zero().counts,
        lexer::LeafCounts::zero().counts,
        0,
        0,
        0
    };

    TypeVisitorState visitor_state = {
        std::move(const_state),
        TypeVisitorState::LevelConstState{
            var_leaf_sizes,
            variant_fields,
            size_leafe_idxs,
            fixed_leaf_counts,
            var_leaf_counts,
            variant_field_counts,
            0,
            0
        },
        mutable_state,
        level_mutable_state
    };

    auto field = target_struct->first_field();
    for (uint16_t i = 0; i < target_struct->field_count; i++) {
        auto field_data = field->data();

        field = TypeVisitor<lexer::StructField, true, false, false>{
            field_data->type(),
            1,
            visitor_state
        }.visit().next_type;
    }
 
    uint64_t offset = 0;

    offset = set_sizes(
        offset,
        fixed_leaf_counts,
        variant_field_counts,
        [&visitor_state](uint16_t i, uint64_t offset) {
            uint64_t& local_offset = visitor_state.const_state.fixed_offsets[i];
            uint64_t size = local_offset;
            logger::debug("i: ", i, ", size: ", size);
            local_offset = offset;
            return offset + size;
        },
        [&variant_fields, &visitor_state](uint16_t i, uint64_t offset) {
            auto variant_field = variant_fields[i];
            for (uint16_t cc_idx = variant_field.start; cc_idx < variant_field.end; cc_idx++) {
                logger::debug("Adding variant (", i, ") base of ", offset, " to leaf cc_idx: ", cc_idx);
                visitor_state.const_state.fixed_offsets[cc_idx] += offset;
            }
            logger::debug("i: ", i, ", variant_field.size: ", variant_field.size);
            return offset + variant_field.size;
        }
    );

    if (total_var_leafs > 0) {
        uint8_t max_var_leaf_align;
        if (var_leaf_counts.size64 > 0) {
            max_var_leaf_align = 8;
        } else if (var_leaf_counts.size32 > 0) {
            max_var_leaf_align = 4;
        } else if (var_leaf_counts.size16 > 0) {
            max_var_leaf_align = 2;
        } else {
            goto done;
        }
        {
            // Add padding based on alignment
            size_t mod = offset % max_var_leaf_align;
            size_t padding = (max_var_leaf_align - mod) & (max_var_leaf_align - 1);
            offset += padding;
        }
        done:;
    }

    for (uint16_t i = 0; i < total_fixed_leafs; i++) {
        logger::debug("Fixed leaf: cc_idx: ", i, ", size: ", visitor_state.const_state.fixed_offsets[i]);
    }
    for (uint16_t i = total_fixed_leafs; i < total_fixed_leafs + total_variant_fixed_leafs; i++) {
        logger::debug("Variant leaf: cc_idx: ", i, ", size: ", visitor_state.const_state.fixed_offsets[i]);
    }

    visitor_state.set_var_offsets(total_var_leafs, level_size_leafs_count);

    logger::debug("var_offset_buffer dump: size: ", var_offset_buffer.current_position(), " {", std::string_view((const char*)var_offset_buffer.c_memory(), var_offset_buffer.current_position()), "}");
    

    return {
        std::move(mutable_state.var_offset_buffer),
        offset
    };
    
}


}