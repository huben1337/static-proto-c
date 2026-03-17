#pragma once

#include <cstdint>
#include <gsl/util>

#include "../../estd/ranges.hpp"
#include "../../core/AlignSizes.hpp"


namespace layout::generation {

struct VariantLeafMeta {
    AlignSizes required_spaces;
    uint64_t used_space = 0;
    AlignCounts left_fields;
    estd::integral_range<uint16_t> field_idxs;
};

} // namespace layout::generation
