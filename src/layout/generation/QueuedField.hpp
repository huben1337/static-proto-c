#pragma once

#include <cstdint>
#include <gsl/util>
#include <variant>

#include "../../parser/lexer_types.hpp"
#include "../../estd/ranges.hpp"

namespace layout::generation {

struct SimpleField {
    uint16_t map_idx;
    SIZE alignment;

    [[nodiscard]] SIZE get_alignment () const { return alignment; }

};

struct VariantFieldPack {
    estd::integral_range<uint16_t> tmp_fixed_offset_idxs;
    SIZE alignment;

    [[nodiscard]] SIZE get_alignment () const { return alignment; }
};

struct ArrayFieldPack {
    estd::integral_range<uint16_t> tmp_fixed_offset_idxs;
    uint16_t pack_info_idx;

    [[nodiscard]] SIZE get_alignment () const {
        return SIZE::from_integral(pack_info_idx);
    }
};

struct QueuedField {

    struct Info : std::variant<SimpleField, ArrayFieldPack, VariantFieldPack> {
        using variant::variant;

        [[nodiscard]] constexpr SIZE alignment (this const Info& self) {
            return std::visit([]<typename T>(const T& arg) -> SIZE {
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

} // namespace layout::generation
