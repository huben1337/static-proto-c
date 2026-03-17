#pragma once

#include <cstdint>

#include "../estd/utility.hpp"
#include "../estd/functional.hpp"
#include "./SIZE.hpp"
#include "./IntegralAlignMembersBase.hpp"
#include "./AlignCounts.hpp"


struct AlignSizes : IntegralAlignMembersBase<uint64_t, SIZE::SIZE_8, SIZE::SIZE_1, AlignSizes> {
    using IntegralAlignMembersBase::IntegralAlignMembersBase;

private:
    template <SIZE... alignments>
    constexpr AlignSizes (const AlignCounts& counts, estd::variadic_v<alignments...> /*unused*/)
        : IntegralAlignMembersBase{counts.get<alignments>()...} {}

public:
    constexpr explicit AlignSizes (const AlignCounts& counts)
        : AlignSizes{counts, AlignCounts::alignments{}} {}

    constexpr AlignSizes& operator += (const AlignSizes& other) {
        return do_compound_binary_operation<estd::add>(other);
    }

    [[nodiscard]] constexpr AlignSizes operator + (const AlignSizes& other) const {
        return do_binary_operation<estd::add>(other);
    }

    [[nodiscard]] constexpr AlignSizes operator * (const uint32_t factor) const {
        return do_binary_operation<estd::multiply>(factor);
    }

    [[nodiscard]] constexpr uint64_t total () const {
        return sum<uint64_t>(alignments{});
    }

    [[nodiscard]] constexpr bool empty () const {
        return total() == 0;
    }


    template <estd::discouraged_annotation>
    [[nodiscard]] static constexpr AlignSizes from_space_at_size (const SIZE size, const uint64_t space) {
        AlignSizes result = AlignSizes::zero();
        result.get<estd::discouraged>(size) = space;
        return result;
    }

    template <estd::discouraged_annotation>
    [[nodiscard]] static constexpr AlignSizes from_size (const SIZE size, const uint32_t count) {
        return from_space_at_size<estd::discouraged>(size, static_cast<uint64_t>(size.byte_size()) * count);
    }

    template <SIZE size>
    [[nodiscard]] static constexpr AlignSizes from_space_at_size (const uint64_t space) {
        AlignSizes result = AlignSizes::zero();
        result.get<size>() = space;
        return result;
    }
    
    template <SIZE size>
    [[nodiscard]] static constexpr AlignSizes from_size (const uint32_t count) {
        return from_space_at_size<size>(static_cast<uint64_t>(size.byte_size()) * count);
    }

    template <SIZE size>
    [[nodiscard]] static consteval AlignSizes from_size () {
        return from_size<size>(1);
    }
};