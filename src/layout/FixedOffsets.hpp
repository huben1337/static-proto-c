#pragma once

#include <cstdint>

#include "../core/SIZE.hpp"
#include "../util/logger.hpp"

namespace layout {

struct FixedOffset {
    uint64_t offset;
    uint16_t map_idx;
    SIZE pack_align;

    constexpr FixedOffset (uint64_t offset, uint16_t map_idx, SIZE pack_align)
        : offset(offset), map_idx(map_idx), pack_align(pack_align) {}

    [[nodiscard]] constexpr SIZE get_pack_align () const {
        return pack_align;
    }

    constexpr void set_pack_align (SIZE value) { pack_align = value; }

    [[nodiscard]] constexpr uint64_t get_offset () const { return offset; }

    constexpr void set_offset (uint64_t value) { offset = value; }

    constexpr void increment_offset (uint64_t value) { offset += value; }

    [[nodiscard]] constexpr bool operator == (const FixedOffset& other) const {
        return offset == other.offset
            && map_idx == other.map_idx
            && pack_align == other.pack_align;
    }

    [[nodiscard]] static consteval FixedOffset empty() {
        return {static_cast<uint64_t>(-1), static_cast<uint16_t>(-1), SIZE::SIZE_0};
    }

    template <typename writer_params>
    void log (const logger::writer<writer_params> w) const {
        w.template write<true, true>("FixedOffset{offset: ", offset, ", map_idx: ", map_idx, ", pack_align: ", pack_align, "}");
    }
};

} // namespace layout::data
