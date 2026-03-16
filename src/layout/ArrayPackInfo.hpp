#pragma once

#include <cstdint>
#include <span>

namespace layout {

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

} // namespace layout::data
