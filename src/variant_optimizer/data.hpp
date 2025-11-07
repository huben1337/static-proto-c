#pragma once

#include <sys/types.h>
#include <bit>
#include <cstdint>
#include <gsl/util>
#include <vector>
#include <variant>

#include "../estd/enum.hpp"
#include "../estd/variant.hpp"
#include "../parser/lexer_types.hpp"
#include "../estd/ranges.hpp"
#include "../estd/integral_pair.hpp"

struct FixedLeaf : private estd::u48_u16_pair {
    FixedLeaf () = default;

    constexpr explicit FixedLeaf (uint64_t data)
    : u48_u16_pair(data)
    {}

    constexpr FixedLeaf (uint64_t size, uint16_t map_idx)
    : u48_u16_pair(size, map_idx)
    {}

    [[nodiscard]] constexpr uint16_t get_map_idx () const { return get_u16(); }

    constexpr void set_map_idx (uint16_t value) { set_u16(value); }

    [[nodiscard]] constexpr uint64_t get_size () const { return get_u48(); }

    constexpr void set_size (uint64_t value) { set_u48(value); }

    constexpr void increment_size_unsafe (uint64_t value) { data += value; };

    [[nodiscard]] constexpr bool operator == (const FixedLeaf& other) const {
        return data == other.data;
    }

    constexpr void set_zero () {
        data = 0;
    }

    [[nodiscard]] constexpr bool is_zero () const {
        return data == 0;
    }
};

struct FixedOffset : private estd::u48_u16_pair {
    FixedOffset () = default;

    constexpr explicit FixedOffset (uint64_t data)
    : u48_u16_pair(data)
    {}

    constexpr FixedOffset (uint64_t offset, lexer::SIZE pack_align)
    : u48_u16_pair(offset, pack_align)
    {}

    [[nodiscard]] constexpr lexer::SIZE get_pack_align () const {
        return std::bit_cast<lexer::SIZE>(gsl::narrow_cast<uint8_t>(get_u16()));
    }

    constexpr void set_pack_align (lexer::SIZE value) { set_u16(value); }

    [[nodiscard]] constexpr uint64_t get_offset () const { return get_u48(); }

    constexpr void set_offset (uint64_t value) { set_u48(value); }

    constexpr void increment_offset (uint64_t value) { data += value; };

    [[nodiscard]] constexpr bool operator == (const FixedOffset& other) const {
        return data == other.data;
    }
};

struct VariantField {
    struct Range : public estd::integral_range<uint16_t> {
        constexpr Range () = default;
        using integral_range::integral_range;

        [[nodiscard]] static consteval Range empty () { return {0, 0}; }
    };
    Range align1;
    Range align2;
    Range align4;
    Range align8;

    lexer::LeafSizes sizes;
};

struct VariantLeafMeta {
    lexer::LeafCounts::Counts fixed_leafs_ends;
    uint16_t fixed_leafs_start;
    estd::integral_range<uint16_t> variant_field_idxs;
    uint64_t required_space;
    lexer::LeafSizes used_spaces;
};

struct FieldTypeTag : estd::ENUM_CLASS<uint8_t> {
    using ENUM_CLASS::ENUM_CLASS;
    static const FieldTypeTag SIMPLE;
    static const FieldTypeTag VARIANT_PACK;
    static const FieldTypeTag ARRAY_PACK;
};

constexpr FieldTypeTag FieldTypeTag::SIMPLE{0};
constexpr FieldTypeTag FieldTypeTag::VARIANT_PACK{1};
constexpr FieldTypeTag FieldTypeTag::ARRAY_PACK{2};



template <FieldTypeTag tag>
using field_tag = estd::tag_template<FieldTypeTag, FieldTypeTag>::type<tag>;


struct SimpleField {
    // field_tag<FieldTypeTag::SIMPLE> tag;
    lexer::SIZE alignment {};
    uint16_t map_idx {};

    constexpr SimpleField () = default;

    constexpr SimpleField (lexer::SIZE alignment, uint16_t map_idx)
    : alignment(alignment), map_idx(map_idx) {}
};

struct FieldPack {
    // field_tag<FieldTypeTag::VARIANT_PACK> tag;
    // lexer::SIZE alignment;
    estd::integral_range<uint16_t> field_idxs;
};

struct QueuedField {

    using info_t = std::variant<SimpleField, FieldPack>;

    uint64_t size {};
    info_t info;

    constexpr QueuedField () = default;

    constexpr QueuedField (uint64_t size, info_t info) : size(size), info(info) {}
};

using fields_t = std::vector<QueuedField>;