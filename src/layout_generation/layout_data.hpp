#pragma once

#include <cstdint>
#include <gsl/util>
#include <variant>

#include "../parser/lexer_types.hpp"
#include "../estd/ranges.hpp"

struct VariantLeafMeta {
    lexer::LeafSizes required_spaces;
    uint64_t used_space = 0;
    lexer::LeafCounts::Counts left_fields;
    estd::integral_range<uint16_t> field_idxs;
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

struct QueuedField {

    struct Info : std::variant<SimpleField, ArrayFieldPack, VariantFieldPack> {
        using variant::variant;

        [[nodiscard]] constexpr lexer::SIZE alignment (this const Info& self) {
            return std::visit([]<typename T>(const T& arg) -> lexer::SIZE {
                if constexpr (
                        std::is_same_v<SimpleField, T>
                    || std::is_same_v<VariantFieldPack, T>
                    || std::is_same_v<ArrayFieldPack, T>
                ) {
                    return arg.get_alignment();
                } else {
                    static_assert(false);
                }
            }, self);
        }
    };

    uint64_t size = static_cast<uint64_t>(-1);
    Info info;

    consteval QueuedField () = default;

    constexpr QueuedField (uint64_t size, Info info) : size(size), info(info) {}
};

struct PendingVariantFieldPacks : lexer::AlignMembersBase<std::pair<uint64_t, estd::integral_range<uint16_t>>, lexer::SIZE::SIZE_8, lexer::SIZE::SIZE_1, PendingVariantFieldPacks> {
    using AlignMembersBase::AlignMembersBase;
};
