#pragma once

#include <bit>
#include <cstdint>
#include <ranges>
#include <array>

#include "../parser/lexer_types.hpp"
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
        return std::bit_cast<lexer::SIZE>(static_cast<uint8_t>(get_u16()));
    }

    constexpr void set_pack_align (lexer::SIZE value) { set_u16(value); }

    [[nodiscard]] constexpr uint64_t get_offset () const { return get_u48(); }

    constexpr void set_offset (uint64_t value) { set_u48(value); }

    constexpr void increment_offset (uint64_t value) { data += value; };

    [[nodiscard]] constexpr bool operator == (const FixedOffset& other) const {
        return data == other.data;
    }
};


struct VariantLeafMeta {
    using ranges_t = std::array<std::ranges::subrange<const FixedLeaf*>, 4>;

    lexer::LeafCounts::Counts fixed_leaf_ends;
    FixedLeaf* fixed_leafs;
    uint64_t required_space;
    lexer::LeafSizes used_spaces;
};