#pragma once
#include "base.cpp"
#include "fatal_error.cpp"
#include "memory.cpp"
#include <cstdint>
#include <array>
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


struct LeafsSharedData {

    LeafsSharedData (
        uint64_t* fixed_offsets,
        uint64_t* variant_offsets,
        uint16_t* idx_map
    ) :
    fixed_offsets(fixed_offsets),
    variant_offsets(variant_offsets),
    idx_map(idx_map),
    current_map_idx(0),
    current_variant_field_idx(0)
    {}

    uint64_t* const fixed_offsets;      // represets the size of each fixed size leaf. once leafs are arranged it represents the offset
    // uint64_t* const var;        // represents the size of the variable size leaf. used for genrating the offset calc strings
    uint64_t* const variant_offsets;
    uint16_t* const idx_map;    // maps occurence in the AST to a stored leaf
    mutable uint16_t current_map_idx;
    mutable uint16_t current_variant_field_idx; 

    uint16_t next_map_idx () const {
        return current_map_idx++;
    }

    void set_next_map_idx_offset (uint64_t offset) const {
        fixed_offsets[next_map_idx()] = offset;
    }

    uint16_t next_variant_field_idx () const {
        return current_variant_field_idx++;
    }

    void set_next_variant_offset (uint64_t offset) const {
        variant_offsets[next_variant_field_idx()] = offset;
    }
};

template <bool in_variant>
struct Leafs {

    // Leafs () = default;
    Leafs (
        const LeafsSharedData& shared,
        uint8_t* const data,
        const lexer::LeafCounts::Counts fixed_leaf_counts,
        const lexer::LeafCounts::Counts var_leaf_counts,
        const lexer::LeafCounts::Counts variant_field_counts,
        const uint16_t total_fixed_leafs,
        const uint16_t total_var_leafs,
        const uint16_t total_size_leafs,
        const uint16_t level_variant_fields,
        const uint16_t fixed_idx_base,
        const uint16_t var_idx_base
    )
    :   shared(shared),
        data(data),
        fixed_leaf_counts(fixed_leaf_counts),
        var_leaf_counts(var_leaf_counts),
        variant_field_counts(variant_field_counts),
        total_fixed_leafs(total_fixed_leafs),
        total_var_leafs(total_var_leafs),
        total_size_leafs(total_size_leafs),
        level_variant_fields(level_variant_fields),
        fixed_idx_base(fixed_idx_base),
        var_idx_base(var_idx_base)
    {   
        if constexpr (in_variant) {
            *level_current_fixed_idx_base() = fixed_idx_base;
            *level_current_var_idx_base() = var_idx_base;
        }
        *current_size_leaf_idx() = 0;
        *fixed_leaf_positions() = {0, 0, 0, 0};
        *var_leaf_positions() = {0, 0, 0, 0};
        *variant_field_positions() = {0, 0, 0, 0};
    }
    
    const LeafsSharedData& shared;
    uint8_t* const data;
    const lexer::LeafCounts::Counts fixed_leaf_counts;
    const lexer::LeafCounts::Counts var_leaf_counts;
    const lexer::LeafCounts::Counts variant_field_counts;
    const uint16_t total_fixed_leafs;
    const uint16_t total_var_leafs;
    const uint16_t total_size_leafs;
    const uint16_t level_variant_fields;
    const uint16_t fixed_idx_base;
    const uint16_t var_idx_base;

    static INLINE size_t mem_size (uint16_t total_var_leafs, uint16_t total_size_leafs, uint16_t level_variant_fields) {
        if constexpr (in_variant) {
            return sizeof(lexer::LeafCounts::Counts) * 3 + sizeof(uint16_t) * 3 + (alignof(VariantField) - sizeof(uint16_t) * 3) + level_variant_fields * sizeof(VariantField) + total_size_leafs * sizeof(SizeLeaf) + total_var_leafs * (sizeof(uint16_t) + sizeof(uint64_t));
        } else {
            return sizeof(lexer::LeafCounts::Counts) * 3 + sizeof(uint16_t) + (alignof(VariantField) - sizeof(uint16_t)) + level_variant_fields * sizeof(VariantField) + total_size_leafs * sizeof(SizeLeaf) + total_var_leafs * (sizeof(uint16_t) + sizeof(uint64_t));
        }
    }

    template <bool is_fixed>
    INLINE const std::pair<const lexer::LeafCounts::Counts, lexer::LeafCounts::Counts* const> get_counts_and_positions () const {
        if constexpr (is_fixed) {
            return {fixed_leaf_counts, fixed_leaf_positions()};
        } else {
            return {var_leaf_counts, var_leaf_positions()};
        }
    }

    template <bool is_fixed>
    INLINE uint16_t _next_idx (uint16_t base_idx) const {
        if constexpr (is_fixed) {
            if constexpr (in_variant) {
                level_current_fixed_idx_base()[0]++;
                logger::debug("base_idx:", base_idx, ", fixed_idx_base:", fixed_idx_base);
            }
            return base_idx + fixed_idx_base;
        } else {
            if constexpr (in_variant) {
                level_current_var_idx_base()[0]++;
                logger::debug("base_idx:", base_idx, ", var_idx_base:", var_idx_base);
            }
            size_leafe_idxs()[base_idx] = next_size_leaf_idx();
            return base_idx;
        }
    }

    template <bool is_fixed>
    INLINE uint16_t next_size8_idx () const {
        auto [counts, positions] = get_counts_and_positions<is_fixed>();
        if (positions->size8 == counts.size8) {
            INTERNAL_ERROR("[Leafs.reserve_size8_idx] position overflow 8 (%d / %d) %s\ncounts.size8: %d\n", positions->size8, counts.size8, is_fixed ? "default" : "array", counts.size8);
        }
        return _next_idx<is_fixed>(counts.size64 + counts.size32 + counts.size16 + positions->size8++);
    }

    template<bool is_fixed>
    INLINE uint16_t next_size16_idx () const {
        auto [counts, positions] = get_counts_and_positions<is_fixed>();
        if (positions->size16 == counts.size16) {
            INTERNAL_ERROR("[Leafs.reserve_size16_idx] position overflow 16 (%d / %d) %s\ncounts.size16: %d\n", positions->size16, counts.size16, is_fixed ? "default" : "array", counts.size16);
        }
        return _next_idx<is_fixed>(counts.size64 + counts.size32 + positions->size16++);
    }

    template<bool is_fixed>
    INLINE uint16_t next_size32_idx () const {
        auto [counts, positions] = get_counts_and_positions<is_fixed>();
        if (positions->size32 == counts.size32) {
            INTERNAL_ERROR("[Leafs.reserve_size32_idx] position overflow 32 (%d / %d) %s\ncounts.size32: %d\n", positions->size32, counts.size32, is_fixed ? "default" : "array", counts.size32);
        }
        return _next_idx<is_fixed>(counts.size64 + positions->size32++);
    }

    template<bool is_fixed>
    INLINE uint16_t next_size64_idx () const {
        auto [counts, positions] = get_counts_and_positions<is_fixed>();
        if (positions->size64 == counts.size64) {
            INTERNAL_ERROR("[Leafs.reserve_size64_idx] position overflow 64 (%d / %d) %s\ncounts.size64: %d\n", positions->size64, counts.size64, is_fixed ? "default" : "array", counts.size64);
        }
        return _next_idx<is_fixed>(positions->size64++);
    }

    /**
     * @brief Add a fixed size leaf to the global offsets array and a mapping to it in the chunk map.
     *
     * @param idx The index of the leaf. (global index)
     * @param size The size of the leaf.
     */
    template<bool is_fixed>
    INLINE void _reserve (uint16_t idx, uint64_t size) const {
        if constexpr (is_fixed) {
            shared.fixed_offsets[idx] = size;
            uint16_t map_idx = shared.next_map_idx();
            logger::debug("map_idx:", map_idx, ", idx:", idx, ", size:", size);
            shared.idx_map[map_idx] = idx;
        } else {
            var_leaf_sizes()[idx] = size;
            uint16_t map_idx = shared.next_map_idx();
            uint16_t mapped_idx = idx + var_idx_base;
            logger::debug("map_idx:", map_idx, ", idx:", idx, ", mapped_idx:", mapped_idx, ", size:", size);
            shared.idx_map[map_idx] = mapped_idx;
        }
    }


    template<bool is_fixed>
    INLINE void _reserve (std::array<uint16_t, 2> idxs, uint64_t size) const {
        
    }

    template<bool is_fixed>
    INLINE void reserve_size8 (uint64_t size) const {
        return _reserve<is_fixed>(next_size8_idx<is_fixed>(), size);
    }

    template<bool is_fixed>
    INLINE void reserve_size16 (uint64_t size) const {
        return _reserve<is_fixed>(next_size16_idx<is_fixed>(), size);
    }

    template<bool is_fixed>
    INLINE void reserve_size32 (uint64_t size) const {
        return _reserve<is_fixed>(next_size32_idx<is_fixed>(), size);
    }

    template<bool is_fixed>
    INLINE void reserve_size64 (uint64_t size) const {
        return _reserve<is_fixed>(next_size64_idx<is_fixed>(), size);
    }

    template <bool is_fixed>
    INLINE uint16_t next_idx (lexer::SIZE alignment) const {
        switch (alignment) {
        case lexer::SIZE::SIZE_1:
            return next_size8_idx<is_fixed>();
        case lexer::SIZE::SIZE_2:
            return next_size16_idx<is_fixed>();
        case lexer::SIZE::SIZE_4:
            return next_size32_idx<is_fixed>();
        case lexer::SIZE::SIZE_8:
            return next_size64_idx<is_fixed>();
        default:
            INTERNAL_ERROR("[reserve_idx] invalid size");
        }
    }

    template <bool is_fixed>
    INLINE auto reserve_next (lexer::SIZE alignment, uint64_t size) const {
        return _reserve<is_fixed>(next_idx<is_fixed>(alignment), size);
    }
    
    INLINE auto reserve_variant_field (lexer::SIZE alignment) const {
        uint16_t idx;
        switch (alignment) {
        case lexer::SIZE::SIZE_1:
            idx = variant_field_counts.size64 + variant_field_counts.size32 + variant_field_counts.size16 + variant_field_positions()->size8++;
            break;
        case lexer::SIZE::SIZE_2:
            idx = variant_field_counts.size64 + variant_field_counts.size32 + variant_field_positions()->size16++;
            break;
        case lexer::SIZE::SIZE_4:
            idx = variant_field_counts.size64 + variant_field_positions()->size32++;
            break;
        case lexer::SIZE::SIZE_8:
            idx = variant_field_positions()->size64++;
            break;
        case lexer::SIZE::SIZE_0:
            INTERNAL_ERROR("[reserve_variant_field] invalid alignment SIZE_0");
        }
        return variant_fields() + idx;
    }

    INLINE lexer::LeafCounts::Counts* fixed_leaf_positions () const {
        return reinterpret_cast<lexer::LeafCounts::Counts*>(reinterpret_cast<size_t>(data));
    }
    INLINE lexer::LeafCounts::Counts* var_leaf_positions () const {
        return reinterpret_cast<lexer::LeafCounts::Counts*>(reinterpret_cast<size_t>(fixed_leaf_positions()) + sizeof(lexer::LeafCounts::Counts));
    }
    INLINE lexer::LeafCounts::Counts* variant_field_positions () const {
        return reinterpret_cast<lexer::LeafCounts::Counts*>(reinterpret_cast<size_t>(var_leaf_positions()) + sizeof(lexer::LeafCounts::Counts));
    }
    INLINE uint16_t* current_size_leaf_idx () const {
        return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(variant_field_positions()) + sizeof(lexer::LeafCounts::Counts));
    }
    INLINE uint16_t* level_current_fixed_idx_base () const requires (in_variant) {
        return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(current_size_leaf_idx()) + sizeof(uint16_t));
    }
    INLINE uint16_t* level_current_var_idx_base () const requires (in_variant) {
        return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(level_current_fixed_idx_base()) + sizeof(uint16_t));
    }
    INLINE VariantField* variant_fields () const {
        if constexpr (in_variant) {
            return reinterpret_cast<VariantField*>(reinterpret_cast<size_t>(level_current_var_idx_base()) + sizeof(uint16_t) + (alignof(VariantField) - sizeof(uint16_t) * 3));
        } else {
            return reinterpret_cast<VariantField*>(reinterpret_cast<size_t>(current_size_leaf_idx()) + sizeof(uint16_t) + (alignof(VariantField) - sizeof(uint16_t)));
        }
    }
    // INLINE SizeLeaf* size_leafs () const {
    //     return reinterpret_cast<SizeLeaf*>(reinterpret_cast<size_t>(variant_fields()) + level_variant_fields * sizeof(VariantField));
    // }
    // INLINE uint16_t* size_leafe_idxs () const {
    //     return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(size_leafs()) + total_size_leafs * sizeof(SizeLeaf));
    // }
    // INLINE std::string_view* var_leaf_base_names () const {
    //     return reinterpret_cast<std::string_view*>(reinterpret_cast<size_t>(variant_fields()) + level_variant_fields * sizeof(VariantField));
    // }
    // INLINE uint16_t* size_leafe_idxs () const {
    //     return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(var_leaf_base_names()) + total_var_leafs * sizeof(std::string_view));
    // }
    INLINE uint64_t* var_leaf_sizes () const {
        return reinterpret_cast<uint64_t*>(reinterpret_cast<size_t>(variant_fields()) + level_variant_fields * sizeof(VariantField));
    }
    INLINE uint16_t* size_leafe_idxs () const {
        return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(var_leaf_sizes()) + total_var_leafs * sizeof(uint64_t));
    }
    INLINE uint16_t next_size_leaf_idx () const {
        return current_size_leaf_idx()[0]++;
    }
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
INLINE Range<uint16_t> get_range (lexer::LeafCounts::Counts counts) {
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
INLINE const uint16_t& get_count (const lexer::LeafCounts::Counts& counts) {
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
INLINE uint64_t set_sizes (uint64_t offset, const lexer::LeafCounts::Counts& fixed_leaf_counts, const lexer::LeafCounts::Counts& variant_field_counts, F_set_offset&& set_offset, F_add_variant_base&& add_variant_base) {
    if constexpr (size == lexer::SIZE::SIZE_0) {
        return offset;
    } else {
        auto [fixed_leafs_start, fixed_leafs_end] = get_range<size>(fixed_leaf_counts);
        auto [variant_field_start, variant_field_end] = get_range<size>(variant_field_counts);
        if constexpr (aligned) {
            for (uint16_t i = fixed_leafs_start; i < fixed_leafs_end; i++) {
                offset = set_offset(i, offset);
            }
            if (get_count<size>(variant_field_counts) > 0) {
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
                offset = next_multiple<size>(offset);
                offset = set_offset(fixed_leafs_start, offset);
                for (uint16_t i = fixed_leafs_start + 1; i < fixed_leafs_end; i++) {
                    offset = set_offset(i, offset);
                }
                if (get_count<size>(variant_field_counts) > 0) {
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
                for (uint16_t i = variant_field_start; i < variant_field_end; i++) {
                    offset = next_multiple<size>(offset);
                    offset = add_variant_base(i, offset);
                }
                return set_sizes<next_smaller_size<size>(), false>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
            }
        }
    }
}

template <bool in_variant>
INLINE void set_var_offsets (
    const uint16_t& total_var_leafs,
    const uint16_t& level_size_leafs_count,
    const uint16_t& var_leaf_base_idx,
    Buffer::View<uint64_t>* const& var_offsets,
    Buffer& var_offset_buffer,
    const Leafs<in_variant>& leafs
) {
    uint64_t size_leafs[level_size_leafs_count];
    memset(size_leafs, 0, level_size_leafs_count * sizeof(uint64_t));
    for (uint16_t i = 0; i < total_var_leafs; i++) {
        logger::debug("Var leaf: cc_idx: ", i, ", size: ", leafs.var_leaf_sizes()[i]);
        uint16_t known_size_leafs = 0;
        for (uint16_t j = 0; j < i; j++) {
            uint16_t size_leaf_idx = leafs.size_leafe_idxs()[j];
            uint64_t* size_leaf = size_leafs + size_leaf_idx;
            if (*size_leaf == 0) {
                known_size_leafs++;
            }
            logger::debug("loop 1: Size leaf #", size_leaf_idx, ": size: ", *size_leaf);
            *size_leaf += leafs.var_leaf_sizes()[j];
        }
        logger::debug("Dump var_leaf_sizes(): {", leafs.var_leaf_sizes()[0]);
        for (uint16_t j = 1; j < total_var_leafs; j++) {
            logger::debug(", ", leafs.var_leaf_sizes()[j]);
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
        logger::debug("Writing var_offset: ", var_leaf_base_idx + i);
        var_offsets[var_leaf_base_idx + i] = Buffer::View<uint64_t>{size_chain_idx, known_size_leafs};
        memset(size_leafs, 0, sizeof(uint64_t) * known_size_leafs);
        // memset(size_leafs, 0, sizeof(uint64_t) * known_size_leafs);
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

template <typename TypeT, bool is_fixed, bool in_array, bool in_variant>
struct TypeVisitor : lexer::TypeVisitorBase<TypeT> {

    INLINE constexpr TypeVisitor (
        const lexer::Type* const& type,
        Buffer& buffer,
        const Leafs<in_variant>& leafs,
        Buffer::View<uint64_t>* const& var_offsets,
        Buffer& var_offset_buffer,
        const uint64_t& outer_array_length,
        uint16_t& current_fixed_idx_base,
        uint16_t& current_var_idx_base
    ) :
    lexer::TypeVisitorBase<TypeT>(type),
    var_offsets(var_offsets),
    leafs(leafs),
    outer_array_length(outer_array_length),
    buffer(buffer),
    var_offset_buffer(var_offset_buffer),
    current_fixed_idx_base(current_fixed_idx_base),
    current_var_idx_base(current_var_idx_base)
    {}

    Buffer::View<uint64_t>* const& var_offsets;
    const Leafs<in_variant>& leafs;
    const uint64_t& outer_array_length;
    Buffer &buffer;
    Buffer& var_offset_buffer;
    uint16_t& current_fixed_idx_base;
    uint16_t& current_var_idx_base;

    

    template <lexer::VALUE_FIELD_TYPE field_type>
    INLINE void on_simple () const {
        leafs.template reserve_next<is_fixed>(lexer::get_type_alignment<field_type>(), lexer::get_type_size<field_type>() * outer_array_length);
    }

    INLINE void on_bool () const override { on_simple<lexer::VALUE_FIELD_TYPE::BOOL>(); }
    INLINE void on_uint8 () const override { on_simple<lexer::VALUE_FIELD_TYPE::UINT8>(); }
    INLINE void on_uint16 () const override { on_simple<lexer::VALUE_FIELD_TYPE::UINT16>(); }
    INLINE void on_uint32 () const override { on_simple<lexer::VALUE_FIELD_TYPE::UINT32>(); }
    INLINE void on_uint64 () const override { on_simple<lexer::VALUE_FIELD_TYPE::UINT64>(); }
    INLINE void on_int8 () const override { on_simple<lexer::VALUE_FIELD_TYPE::INT8>(); }
    INLINE void on_int16 () const override { on_simple<lexer::VALUE_FIELD_TYPE::INT16>(); }
    INLINE void on_int32 () const override { on_simple<lexer::VALUE_FIELD_TYPE::INT32>(); }
    INLINE void on_int64 () const override { on_simple<lexer::VALUE_FIELD_TYPE::INT64>(); }
    INLINE void on_float32 () const override { on_simple<lexer::VALUE_FIELD_TYPE::FLOAT32>(); }
    INLINE void on_float64 () const override { on_simple<lexer::VALUE_FIELD_TYPE::FLOAT64>(); }

    INLINE void on_fixed_string (const lexer::FixedStringType* const fixed_string_type) const override {
        auto& length = fixed_string_type->length;
        leafs.template reserve_size8<is_fixed>(outer_array_length * length);
    }

    INLINE void on_string (const lexer::StringType* const string_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Variable length strings in arrays not supported");
        } else {
            auto& size_size = string_type->size_size;
            auto& stored_size_size = string_type->stored_size_size;
            leafs.template reserve_size8<false>(1);

            uint16_t leaf_idx = leafs.template next_idx<true>(stored_size_size);
            logger::debug("STRING.size leaf_idx: ", leaf_idx);
            leafs.shared.fixed_offsets[leaf_idx] = lexer::byte_size_of(stored_size_size);

            uint16_t map_idx = leafs.shared.next_map_idx();
            leafs.shared.idx_map[map_idx] = leaf_idx;
        }
    }

    INLINE TypeVisitor::ResultT on_fixed_array (const lexer::ArrayType* const fixed_array_type) const override {
        auto& length = fixed_array_type->length;

        return TypeVisitor<TypeT, is_fixed, true, in_variant>{
            fixed_array_type->inner_type(),
            buffer,
            leafs,
            var_offsets,
            var_offset_buffer,
            fixed_array_type->length * outer_array_length,
            current_fixed_idx_base,
            current_var_idx_base
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
                buffer,
                leafs,
                var_offsets,
                var_offset_buffer,
                1,
                current_fixed_idx_base,
                current_var_idx_base
            }.visit();

            uint16_t leaf_idx = leafs.template next_idx<true>(stored_size_size);
            leafs.shared.fixed_offsets[leaf_idx] = lexer::byte_size_of(stored_size_size);

            uint16_t map_idx = leafs.shared.next_map_idx();
            leafs.shared.idx_map[map_idx] = leaf_idx;

            return next_type;
        }
    }

    INLINE TypeVisitor::ResultT on_fixed_variant (const lexer::FixedVariantType* const fixed_variant_type) const override {
        auto& variant_count = fixed_variant_type->variant_count;

        logger::debug("[variant_id] outer_array_length: ", outer_array_length);
        if (variant_count <= UINT8_MAX) {
            leafs.template reserve_size8<is_fixed>(outer_array_length);
        } else {
            leafs.template reserve_size16<is_fixed>(outer_array_length * 2);
        }              

        auto type = fixed_variant_type->first_variant();

        uint16_t level_current_fixed_idx_base = current_fixed_idx_base;
        uint16_t level_fixed_idx_start = current_fixed_idx_base;
        uint16_t level_fixed_idx_end = level_fixed_idx_start + fixed_variant_type->level_fixed_leafs;
        current_fixed_idx_base = level_fixed_idx_end;

        logger::debug("level fixed_idx range: ", level_fixed_idx_start, " - ", level_fixed_idx_end);

        uint16_t variant_field_idx = leafs.shared.next_variant_field_idx();

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

            uint8_t data[Leafs<true>::mem_size(total_var_leafs, total_size_leafs, level_variant_fields)];
            Leafs<true> variant_leafs = {
                leafs.shared,
                data,
                fixed_leaf_counts,
                var_leaf_counts,
                variant_field_counts,
                total_fixed_leafs,
                total_var_leafs,
                total_size_leafs,
                level_variant_fields,
                level_current_fixed_idx_base,
                0
            };
            type = TypeVisitor<lexer::Type, is_fixed, in_array, true>{
                type,
                buffer,
                variant_leafs,
                var_offsets,
                var_offset_buffer,
                1,
                current_fixed_idx_base,
                current_var_idx_base
            }.visit().next_type;

            uint64_t offset = set_sizes(
                0,
                fixed_leaf_counts,
                variant_field_counts,
                [&variant_leafs, level_current_fixed_idx_base](uint16_t i, uint64_t offset) {
                    uint16_t cc_idx = i + level_current_fixed_idx_base;
                    uint64_t size = variant_leafs.shared.fixed_offsets[cc_idx];
                    variant_leafs.shared.fixed_offsets[cc_idx] = offset;
                    return offset + size;
                },
                [&variant_leafs](uint16_t i, uint64_t offset) {
                    auto variant_field = variant_leafs.variant_fields()[i];
                    for (uint16_t cc_idx = variant_field.start; cc_idx < variant_field.end; cc_idx++) {
                        logger::debug("Adding varaint (", i, ") base of ", offset, " to leaf cc_idx: ", cc_idx);
                        variant_leafs.shared.fixed_offsets[cc_idx] += offset;
                    }
                    return offset + variant_field.size;
                }
            );
            max_offset = std::max(max_offset, offset);

            level_current_fixed_idx_base = *variant_leafs.level_current_fixed_idx_base();
        }

        uint64_t max_size = in_array ? lexer::next_multiple(max_offset, fixed_variant_type->max_alignment) * outer_array_length : max_offset;

        leafs.shared.variant_offsets[variant_field_idx] = max_offset;

        *leafs.reserve_variant_field(fixed_variant_type->max_alignment) = VariantField{max_size, level_fixed_idx_start, current_fixed_idx_base};

        logger::debug("current_fixed_idx_base: ", current_fixed_idx_base, ", max_offset: ", max_offset, ", start_variant_idx: ", level_fixed_idx_start, ", level_current_fixed_idx_base: ", level_current_fixed_idx_base, ", end_variant_idx: ", level_fixed_idx_end);

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
                leafs.template reserve_size8<is_fixed>(outer_array_length);
            } else {
                leafs.template reserve_size16<is_fixed>(outer_array_length * 2);
            }

            auto& stored_size_size = dynamic_variant_type->stored_size_size;
            uint16_t leaf_idx = leafs.template next_idx<true>(stored_size_size);
            logger::debug("VARIANT size leaf_idx: ", leaf_idx);
            logger::debug("DYNAMIC_VARIANT stored_size_size: ", stored_size_size);
            leafs.shared.fixed_offsets[leaf_idx] = lexer::byte_size_of(stored_size_size);

            uint16_t map_idx = leafs.shared.next_map_idx();
            leafs.shared.idx_map[map_idx] = leaf_idx;

            leafs.template reserve_next<false>(dynamic_variant_type->max_alignment, 1);
            // uint16_t size_leaf_idx = leafs.next_size_leaf_idx();
            // logger::debug("VARIANT size_leaf_idx: ", size_leaf_idx);
            // leafs.size_leafs()[size_leaf_idx] = {variant_type->min_byte_size, leaf_idx, variant_type->size_size, stored_size_size};
            // leafs.size_leafs()[size_leaf_idx] = {leaf_idx};

            auto type = dynamic_variant_type->first_variant();

            uint16_t level_current_fixed_idx_base = current_fixed_idx_base;
            uint16_t level_fixed_idx_start = current_fixed_idx_base;
            uint16_t level_fixed_idx_end = level_fixed_idx_start + dynamic_variant_type->level_fixed_leafs;
            current_fixed_idx_base = level_fixed_idx_end;

            uint16_t level_current_var_idx_base = current_var_idx_base;
            uint16_t level_var_idx_start = current_var_idx_base;
            uint16_t level_var_idx_end = level_var_idx_start + dynamic_variant_type->level_var_leafs;
            current_var_idx_base = level_var_idx_end;

            

            logger::debug("level fixed_idx range: [", level_fixed_idx_start, ", ", level_fixed_idx_end, ")");
            logger::debug("level var_idx range: [", level_var_idx_start, ", ", level_var_idx_end, ")");
            uint64_t max_offset = 0;
            uint64_t min_offset = UINT64_MAX;

            uint16_t variant_field_idx = leafs.shared.next_variant_field_idx();
            
            for (uint16_t i = 0; i < variant_count; i++) {
                auto& type_meta = dynamic_variant_type->type_metas()[i];
                auto& fixed_leaf_counts = type_meta.fixed_leaf_counts.counts;
                auto& var_leaf_counts = type_meta.var_leaf_counts.counts;
                auto& variant_field_counts = type_meta.variant_field_counts.counts;
                auto& level_size_leafs_count = type_meta.level_size_leafs;
                const auto total_fixed_leafs = fixed_leaf_counts.total();
                const auto total_var_leafs = var_leaf_counts.total();
                const auto level_variant_fields = variant_field_counts.total();
                auto& total_size_leafs = type_meta.level_size_leafs;

                uint8_t data[Leafs<true>::mem_size(total_var_leafs, total_size_leafs, level_variant_fields)];
                Leafs<true> variant_leafs = {
                    leafs.shared,
                    data,
                    fixed_leaf_counts,
                    var_leaf_counts,
                    variant_field_counts,
                    total_fixed_leafs,
                    total_var_leafs,
                    total_size_leafs,
                    level_variant_fields,
                    level_current_fixed_idx_base,
                    level_current_var_idx_base
                };
                type = TypeVisitor<const lexer::Type, true, in_array, true>{
                    type,
                    buffer,
                    variant_leafs,
                    var_offsets,
                    var_offset_buffer,
                    1,
                    current_fixed_idx_base,
                    current_var_idx_base
                }.visit().next_type;
                // set_size_leafs(variant_leafs);

                uint64_t offset = 0;
                offset = set_sizes(
                    offset,
                    fixed_leaf_counts,
                    variant_field_counts,
                    [&variant_leafs, level_current_fixed_idx_base](uint16_t i, uint64_t offset) {
                        uint16_t cc_idx = i + level_current_fixed_idx_base;
                        uint64_t size = variant_leafs.shared.fixed_offsets[cc_idx];
                        variant_leafs.shared.fixed_offsets[cc_idx] = offset;
                        return offset + size;
                    },
                    [&variant_leafs](uint16_t i, uint64_t offset) {
                        auto variant_field = variant_leafs.variant_fields()[i];
                        for (uint16_t cc_idx = variant_field.start; cc_idx < variant_field.end; cc_idx++) {
                            logger::debug("Adding varaint (", i, ") base of ", offset, " to leaf cc_idx: ", cc_idx);
                            // variant_leafs.shared.fixed_offsets[cc_idx] += offset;
                        }
                        return offset + variant_field.size;
                    }
                );

                set_var_offsets(
                    total_var_leafs,
                    level_size_leafs_count,
                    level_current_var_idx_base,
                    var_offsets,
                    var_offset_buffer,
                    variant_leafs
                );

                max_offset = std::max(max_offset, offset);
                min_offset = std::min(min_offset, offset);

                level_current_fixed_idx_base = *variant_leafs.level_current_fixed_idx_base();
                level_current_var_idx_base = *variant_leafs.level_current_var_idx_base();
            }

            if constexpr (in_array) {
                max_offset *= outer_array_length;
            }

            leafs.shared.variant_offsets[variant_field_idx] = max_offset;

            logger::debug("current_fixed_idx_base: ", current_fixed_idx_base, ", max_offset: ", max_offset, ", start_variant_idx: ", level_fixed_idx_start, ", level_current_fixed_idx_base: ", level_current_fixed_idx_base, ", end_variant_idx: ", level_fixed_idx_end);
            
            return reinterpret_cast<TypeVisitor::ConstTypeT*>(type);
        }
    }

    INLINE void on_identifier (const lexer::IdentifiedType* const identified_type) const override {
        auto* const identifier = buffer.get(identified_type->identifier_idx);
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
                buffer,
                leafs,
                var_offsets,
                var_offset_buffer,
                outer_array_length,
                current_fixed_idx_base,
                current_var_idx_base
            }.visit().next_type;
        }
    }
};

struct ResultInfo {
    uint64_t var_leafs_start;
};

ResultInfo generate (
    const lexer::StructDefinition*& target_struct,
    Buffer& buffer,
    const Offsets& offsets,
    Buffer& var_offset_buffer,
    const lexer::LeafCounts::Counts& fixed_leaf_counts,
    const lexer::LeafCounts::Counts& var_leaf_counts,
    const lexer::LeafCounts::Counts& variant_field_counts,
    const uint16_t& total_fixed_leafs,
    const uint16_t& total_var_leafs,
    const uint16_t& level_variant_fields,
    const uint16_t& level_size_leafs_count
) {
    uint16_t total_variant_fixed_leafs = target_struct->total_variant_fixed_leafs;
    uint16_t total_leafs = total_fixed_leafs + total_var_leafs + total_variant_fixed_leafs;
    uint16_t variant_base_idx = total_fixed_leafs;

    // uint64_t var_leaf_sizes[total_var_leafs];
    const LeafsSharedData leafs_shared = {offsets.fixed_offsets, offsets.variant_offsets, offsets.idx_map};

    alignas(alignof(uint64_t)) uint8_t data[Leafs<false>::mem_size(total_var_leafs, level_size_leafs_count, level_variant_fields)];
    const Leafs<false> leafs = {
        leafs_shared,
        data,
        fixed_leaf_counts,
        var_leaf_counts,
        variant_field_counts,
        total_fixed_leafs,
        total_var_leafs,
        level_size_leafs_count,
        level_variant_fields,
        0,
        0
    };

    uint16_t current_fixed_idx_base = total_fixed_leafs;
    uint16_t current_var_idx_base = total_var_leafs;

    auto field = target_struct->first_field();
    for (uint16_t i = 0; i < target_struct->field_count; i++) {
        auto field_data = field->data();

        field = TypeVisitor<lexer::StructField, true, false, false>{
            field_data->type(),
            buffer,
            leafs,
            offsets.var_offsets,
            var_offset_buffer,
            1,
            current_fixed_idx_base,
            current_var_idx_base
        }.visit().next_type;
    }
 
    uint64_t offset = 0;

    offset = set_sizes(
        offset,
        fixed_leaf_counts,
        variant_field_counts,
        [leafs_shared](uint16_t i, uint64_t offset) {
            uint64_t* size_ptr = leafs_shared.fixed_offsets + i;
            uint64_t size = *size_ptr;
            logger::debug("i: ", i, ", size: ", size);
            *size_ptr = offset;
            return offset + size;
        },
        [&leafs, &leafs_shared](uint16_t i, uint64_t offset) {
            auto variant_field = leafs.variant_fields()[i];
            for (uint16_t cc_idx = variant_field.start; cc_idx < variant_field.end; cc_idx++) {
                logger::debug("Adding variant (", i, ") base of ", offset, " to leaf cc_idx: ", cc_idx);
                leafs_shared.fixed_offsets[cc_idx] += offset;
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
        logger::debug("Fixed leaf: cc_idx: ", i, ", size: ", leafs_shared.fixed_offsets[i]);
    }
    for (uint16_t i = total_fixed_leafs; i < total_fixed_leafs + total_variant_fixed_leafs; i++) {
        logger::debug("Variant leaf: cc_idx: ", i, ", size: ", leafs_shared.fixed_offsets[i]);
    }
    set_var_offsets(
        total_var_leafs,
        level_size_leafs_count,
        0,
        offsets.var_offsets,
        var_offset_buffer,
        leafs
    );
    

    return {offset};
    
}


}