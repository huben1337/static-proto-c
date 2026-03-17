#pragma once

#include <concepts>
#include <limits>

#include "../estd/utility.hpp"
#include "../estd/functional.hpp"
#include "./SIZE.hpp"
#include "./AlignMembersBase.hpp"

template<std::integral T, SIZE max_alignment = SIZE::SIZE_8, SIZE min_alignment = SIZE::SIZE_1, typename Outside = void>
struct IntegralAlignMembersBase : AlignMembersBase<T, max_alignment, min_alignment, Outside> {
private:
    using Base = AlignMembersBase<T, max_alignment, min_alignment, Outside>;
    using Base::Base;

public:
    friend Outside;

    IntegralAlignMembersBase () = delete;

private:
    using outside_t = Base::outside_t;

    template <SIZE... alignments_to_check>
    requires (sizeof...(alignments_to_check) > 0)
    [[nodiscard]] constexpr SIZE _largest_align (estd::variadic_v<alignments_to_check...> /*unused*/) const {
        SIZE result;
        const bool checked_matched = (((Base::template get<alignments_to_check>() != 0) ? (result = alignments_to_check, true) : false) || ...);
        if (checked_matched) return result;
        return SIZE::SIZE_1;
    }

public:
    template <SIZE limit = max_alignment>
    requires (limit >= min_alignment && limit <= max_alignment)
    [[nodiscard]] constexpr SIZE largest_align () const {
        if constexpr (min_alignment == max_alignment) {
            return min_alignment;
        } else {
            return _largest_align(typename make_size_range<min_alignment.next_bigger(), max_alignment>
                    ::template apply<estd::reverse_variadic_v_t>{});
        }
    }

private:
    template <typename Operator, SIZE... alignments>
    [[nodiscard]] constexpr outside_t _do_binary_operation (const outside_t& other, estd::variadic_v<alignments...> /*unused*/) const {
        return outside_t{
            static_cast<T>(Operator::apply(Base::template get<alignments>(), other.template get<alignments>())) ...
        };
    }

    template <typename Operator, std::integral U, SIZE... alignments>
    [[nodiscard]] constexpr outside_t _do_binary_operation (const U& other, estd::variadic_v<alignments...> /*unused*/) const {
        return outside_t{
            static_cast<T>(Operator::apply(Base::template get<alignments>(), other)) ...
        };
    }

    template <typename Operator, typename U, SIZE... alignments>
    [[nodiscard]] constexpr outside_t do_binary_operation (const U& other) const {
        return _do_binary_operation<Operator>(other, typename Base::alignments{});
    }

    template <typename Operator, SIZE... alignments>
    [[nodiscard]] constexpr outside_t& _do_compound_binary_operation (this outside_t& self, const outside_t& other, estd::variadic_v<alignments...> /*unused*/) {
        (estd::compound_asign<Operator>(self.template get<alignments>(), other.template get<alignments>()), ...);
        return self;
    }

    template <typename Operator, std::integral U, SIZE... alignments>
    [[nodiscard]] constexpr outside_t& _do_compound_binary_operation (this outside_t& self, const U& other, estd::variadic_v<alignments...> /*unused*/) {
        (estd::compound_asign<Operator>(self.template get<alignments>(), other), ...);
        return self;
    }

    template <typename Operator, typename U, SIZE... alignments>
    [[nodiscard]] constexpr outside_t& do_compound_binary_operation (this outside_t& self, const U& other) {
        return self.template _do_compound_binary_operation<Operator>(other, typename Base::alignments{});
    }

public:
    template <std::integral U, SIZE... alignments>
    [[nodiscard]] constexpr U sum (estd::variadic_v<alignments...> /*unused*/) const {
        static_assert(
            std::numeric_limits<T>::min() >= std::numeric_limits<U>::min() &&
            std::numeric_limits<T>::max() <= std::numeric_limits<U>::max()
        );
        return (... + static_cast<U>(Base::template get<alignments>()));
    }

    template <typename Result = outside_t>
    [[nodiscard]] static consteval Result zero () { return Base::template of<Result>(0); }
};