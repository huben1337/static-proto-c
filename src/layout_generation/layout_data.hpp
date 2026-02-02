#pragma once

#include <cstdint>
#include <gsl/util>
#include <variant>

#include "../parser/lexer_types.hpp"
#include "../estd/ranges.hpp"
#include "../estd/integral_pair.hpp"

struct VariantLeafMeta {
    lexer::LeafSizes used_spaces;
    uint64_t required_space;
    lexer::LeafCounts::Counts left_fields;
    estd::integral_range<uint16_t> field_idxs;
    
    // template <lexer::SIZE alignment>
    // [[nodiscard]] constexpr uint16_t fields_begin_idx () {
    //     if constexpr (alignment == lexer::SIZE::SIZE_8) {
    //         return start;
    //     } else {
    //         return ends.get<alignment.next_bigger()>();
    //     }
    // }
};

struct SimpleField {
    uint16_t map_idx;
    lexer::SIZE alignment;

    [[nodiscard]] lexer::SIZE get_alignment () const { return alignment; }

};

struct VariantFieldPack {
    estd::integral_range<uint16_t> tmp_fixed_offset_idxs;
    lexer::SIZE alignment;

    [[nodiscard]] lexer::SIZE get_alignment () const { return alignment; }
};

struct ArrayFieldPack {
    estd::integral_range<uint16_t> tmp_fixed_offset_idxs;
    uint16_t pack_info_idx;

    [[nodiscard]] lexer::SIZE get_alignment () const {
        return lexer::SIZE::from_integral(pack_info_idx);
    }
};


struct FieldSize : private estd::u48_u16_pair {
    FieldSize () = default;

    constexpr explicit FieldSize (uint64_t data)
        : u48_u16_pair(data) {}

    constexpr FieldSize (uint64_t offset, bool flag)
        : u48_u16_pair(offset, static_cast<uint16_t>(flag)) {}

    [[nodiscard]] constexpr bool get_flag () const {
        return gsl::narrow_cast<bool>(get_u16());
    }

    constexpr void set_flag (bool value) { set_u16(static_cast<uint16_t>(value)); }

    [[nodiscard]] constexpr uint64_t get_size () const { return get_u48(); }

    constexpr void set_size (uint64_t value) { set_u48(value); }

    // constexpr void increment_offset (uint64_t value) { data += value; };

    [[nodiscard]] constexpr bool operator == (const FieldSize& other) const {
        return data == other.data;
    }
};

struct QueuedField {

    using info_t = std::variant<SimpleField, ArrayFieldPack, VariantFieldPack>;

    uint64_t size {};
    info_t info;

    constexpr QueuedField () = default;

    constexpr QueuedField (uint64_t size, info_t info) : size(size), info(info) {}
};

struct PendingVariantFieldPacks : lexer::AlignMembersBase<std::pair<uint64_t, estd::integral_range<uint16_t>>, lexer::SIZE::SIZE_8, PendingVariantFieldPacks> {
    using AlignMembersBase::AlignMembersBase;
};
