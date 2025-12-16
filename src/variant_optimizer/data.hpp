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

struct FixedOffsetOld : private estd::u48_u16_pair {
    FixedOffsetOld () = default;

    constexpr explicit FixedOffsetOld (uint64_t data)
    : u48_u16_pair(data)
    {}

    constexpr FixedOffsetOld (uint64_t offset, lexer::SIZE pack_align)
    : u48_u16_pair(offset, pack_align)
    {}

    [[nodiscard]] constexpr lexer::SIZE get_pack_align () const {
        return std::bit_cast<lexer::SIZE>(gsl::narrow_cast<uint8_t>(get_u16()));
    }

    constexpr void set_pack_align (lexer::SIZE value) { set_u16(value); }

    [[nodiscard]] constexpr uint64_t get_offset () const { return get_u48(); }

    constexpr void set_offset (uint64_t value) { set_u48(value); }

    constexpr void increment_offset (uint64_t value) { data += value; };

    [[nodiscard]] constexpr bool operator == (const FixedOffsetOld& other) const {
        return data == other.data;
    }
};

struct FixedOffset {
    FixedOffset () = default;

    uint64_t offset;
    uint16_t map_idx;
    lexer::SIZE pack_align;

    constexpr FixedOffset (uint64_t offset, uint16_t map_idx, lexer::SIZE pack_align)
        : offset(offset), map_idx(map_idx), pack_align(pack_align) {}

    [[nodiscard]] constexpr lexer::SIZE get_pack_align () const {
        return pack_align;
    }

    constexpr void set_pack_align (lexer::SIZE value) { pack_align = value; }

    [[nodiscard]] constexpr uint64_t get_offset () const { return offset; }

    constexpr void set_offset (uint64_t value) { offset = value; }

    constexpr void increment_offset (uint64_t value) { offset += value; };

    [[nodiscard]] constexpr bool operator == (const FixedOffset& other) const {
        return offset == other.offset
            && map_idx == other.map_idx
            && pack_align == other.pack_align;
    }

    [[nodiscard]] static consteval FixedOffset empty() {
        return  {static_cast<uint64_t>(-1), static_cast<uint16_t>(-1), lexer::SIZE::SIZE_0};
    }

    template <typename writer_params>
    void log (logger::writer<writer_params> w) const {
        w.template write<true, true>("FixedOffset{offset: ", offset, ", map_idx: ", map_idx, ", pack_align: ", pack_align, "}");
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
    uint16_t start;
    lexer::LeafCounts::Counts ends;
    uint64_t required_space;
    lexer::LeafSizes used_spaces;

    template <lexer::SIZE alignment>
    [[nodiscard]] constexpr uint16_t fields_begin_idx () {
        if constexpr (alignment == lexer::SIZE::SIZE_8) {
            return start;
        } else {
            return ends.get<alignment.next_bigger()>();
        }
    }

    template <lexer::SIZE alignment>
    [[nodiscard]] constexpr uint16_t fields_end_idx () {
        return ends.get<alignment>();
    }
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
    //lexer::SIZE alignment {};
    uint16_t map_idx {};

    constexpr SimpleField () = default;

    constexpr explicit SimpleField (uint16_t map_idx)
    : map_idx(map_idx) {}
};

struct VariantFieldPack {
    estd::integral_range<uint16_t> tmp_fixed_offset_idxs;
};

struct ArrayFieldPack {
    // field_tag<FieldTypeTag::VARIANT_PACK> tag;
    // lexer::SIZE alignment;
    estd::integral_range<uint16_t> tmp_fixed_offset_idxs;
    uint16_t pack_info_idx;
};

struct ArrayPackInfo {
    uint64_t size;
    uint16_t parent_idx;

    [[nodiscard]] bool has_parent () const {
        return parent_idx != static_cast<uint16_t>(-1);
    }

    [[nodiscard]] const ArrayPackInfo& get_parent (const std::span<const ArrayPackInfo>& pack_infos) const {
        return pack_infos[parent_idx];
    }
};

struct SkippedField {};

struct QueuedField {

    using info_t = std::variant<SimpleField, ArrayFieldPack, VariantFieldPack, SkippedField>;

    uint64_t size {};
    info_t info;

    constexpr QueuedField () = default;

    constexpr QueuedField (uint64_t size, info_t info) : size(size), info(info) {}
};

using fields_t = std::vector<QueuedField>;