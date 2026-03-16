#pragma once

#include <cstdint>
#include <gsl/util>

#include "../../parser/lexer_types.hpp"
#include "../../estd/ranges.hpp"

namespace layout::generation {

struct VariantLeafMeta {
    lexer::LeafSizes required_spaces;
    uint64_t used_space = 0;
    lexer::LeafCounts::Counts left_fields;
    estd::integral_range<uint16_t> field_idxs;
};

} // namespace layout::generation
