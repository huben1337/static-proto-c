#pragma once

#include "../../core/AlignMembersBase.hpp"
#include "../../estd/ranges.hpp"

namespace layout::generation {

struct PendingVariantFieldPacks : AlignMembersBase<std::pair<uint64_t, estd::integral_range<uint16_t>>, SIZE::SIZE_8, SIZE::SIZE_1, PendingVariantFieldPacks> {
    using AlignMembersBase::AlignMembersBase;
};

}// namespace layout::generation::variant_layout