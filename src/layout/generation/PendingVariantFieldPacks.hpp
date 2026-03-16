#pragma once

#include "../../parser/lexer_types.hpp"
#include "../../estd/ranges.hpp"

namespace layout::generation {

struct PendingVariantFieldPacks : lexer::AlignMembersBase<std::pair<uint64_t, estd::integral_range<uint16_t>>, lexer::SIZE::SIZE_8, lexer::SIZE::SIZE_1, PendingVariantFieldPacks> {
    using AlignMembersBase::AlignMembersBase;
};

}// namespace layout::generation::variant_layout