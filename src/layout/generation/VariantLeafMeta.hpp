#pragma once

#include <cstdint>
#include <gsl/util>

#include "../../estd/ranges.hpp"
#include "../../core/AlignSizes.hpp"


namespace layout::generation {

struct VariantLeafMeta {
    AlignSizes required_spaces = AlignSizes::zero();
    uint64_t used_space = static_cast<uint64_t>(-1);
    AlignCounts left_fields = AlignCounts::zero();
    estd::integral_range<uint16_t> field_idxs;
};

} // namespace layout::generation
