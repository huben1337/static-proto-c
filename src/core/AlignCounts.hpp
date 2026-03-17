#pragma once

#include <cstdint>

#include "./SIZE.hpp"
#include "./IntegralAlignMembersBase.hpp"


struct AlignCounts : IntegralAlignMembersBase<uint16_t, SIZE::SIZE_8, SIZE::SIZE_1, AlignCounts> {
    using IntegralAlignMembersBase::IntegralAlignMembersBase;

    [[nodiscard]] constexpr uint16_t total () const {
        return sum<uint16_t>(alignments{});
    }
};