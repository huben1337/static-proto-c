#pragma once

#include <utility>
#include <compare>
#include <concepts>
#include <type_traits>

#include "../estd/utility.hpp"
#include "../estd/enum.hpp"

struct SIZE : estd::ENUM_CLASS<uint8_t, SIZE> {

    using ENUM_CLASS::ENUM_CLASS;

    template <value_t v>
    struct Mapped;
    
    static const SIZE SIZE_1;
    static const SIZE SIZE_2;
    static const SIZE SIZE_4;
    static const SIZE SIZE_8;
    static const SIZE SIZE_0;

    static const SIZE MIN;
    static const SIZE MAX;

    constexpr SIZE () : SIZE{SIZE_0} {};

    [[nodiscard]] constexpr std::strong_ordering operator <=> (this const SIZE& self, const SIZE& other) {
        return self.ordinal() <=> other.ordinal();
    }

    [[nodiscard]] constexpr SIZE operator - (this const SIZE& self, const SIZE& other) {
        return SIZE{gsl::narrow_cast<value_t>(self.ordinal() - other.ordinal())};
    }

    [[nodiscard]] constexpr SIZE operator + (this const SIZE& self, const SIZE& other) {
        return SIZE{gsl::narrow_cast<value_t>(self.ordinal() + other.ordinal())};
    }

    [[nodiscard]] constexpr value_t byte_size () const {
        return value_t{1} << ordinal();
    }

    template <typename Result, auto... target_sizes, typename... U, typename Visitor>
    [[nodiscard]] constexpr Result visit (this const SIZE& self, estd::variadic_v<target_sizes...> /*unused*/, Visitor&& visitor, U&&... args);

private:
    static void next_smaller_is_invalid_for_this_size () {}
    static void next_bigger_is_invalid_for_this_size () {}

public:
    [[nodiscard]] consteval SIZE next_smaller (this const SIZE& self) {
        if (self == SIZE_0 || self == SIZE_1) {
            next_smaller_is_invalid_for_this_size();
        }
        return SIZE{gsl::narrow_cast<value_t>(self.ordinal() - 1)};
    };

    [[nodiscard]] consteval SIZE next_bigger (this const SIZE& self) {
        if (self == SIZE_0 || self == SIZE_8) {
            next_bigger_is_invalid_for_this_size();
        }
        return SIZE{gsl::narrow_cast<value_t>(self.ordinal() + 1)};
    };

    template <std::unsigned_integral T>
    [[nodiscard]] static SIZE from_integral (const T i) {
        return SIZE{gsl::narrow_cast<value_t>(i % gsl::narrow_cast<value_t>(MAX.ordinal() + 1))};
    }

    template <typename writer_params>
    void log (const logger::writer<writer_params> w) const {
        w.template write<true, true>(outside_name + "_"_sl, byte_size());
    }

    struct next;

    struct enums;
};

template<SIZE min, SIZE max>
using make_size_range = estd::make_index_range<min.ordinal(), max.ordinal() + 1>::template map<SIZE::Mapped>;


constexpr SIZE SIZE::SIZE_1{0};
constexpr SIZE SIZE::SIZE_2{1};
constexpr SIZE SIZE::SIZE_4{2};
constexpr SIZE SIZE::SIZE_8{3};
constexpr SIZE SIZE::SIZE_0{static_cast<value_t>(-1)};
constexpr SIZE SIZE::MIN   {SIZE::SIZE_1};
constexpr SIZE SIZE::MAX   {SIZE::SIZE_8};

struct SIZE::enums : estd::variadic_v<SIZE_1, SIZE_2, SIZE_4, SIZE_8> {};

template <SIZE::value_t value_>
struct SIZE::Mapped {
    static constexpr SIZE value {value_};

private:
    template <auto... w>
    using includes_value = std::bool_constant<((w == value) || ...)>;

    static_assert(
        value == SIZE::SIZE_0 ||
        SIZE::enums::apply<includes_value>::value
    );
};

// namespace _size_detail {
//     template <typename Visitor, SIZE size>
//     concept size_visitor_applicable = requires (Visitor&& visitor) {
//         { visitor.template operator()<size>() } -> estd::conceptify<estd::is_any_t>;
//     };
// }

template <typename Result, auto... target_sizes, typename... U, typename Visitor>
[[nodiscard]] constexpr Result SIZE::visit (this const SIZE& self, estd::variadic_v<target_sizes...> /*unused*/, Visitor&& visitor, U&&... args) {
    switch (self.ordinal()) {
        case SIZE_1.ordinal(): if constexpr (((target_sizes == SIZE_1) || ...)) { return std::forward<Visitor>(visitor).template operator()<SIZE_1>(std::forward<U>(args)...); } else { std::unreachable(); };
        case SIZE_2.ordinal(): if constexpr (((target_sizes == SIZE_2) || ...)) { return std::forward<Visitor>(visitor).template operator()<SIZE_2>(std::forward<U>(args)...); } else { std::unreachable(); };
        case SIZE_4.ordinal(): if constexpr (((target_sizes == SIZE_4) || ...)) { return std::forward<Visitor>(visitor).template operator()<SIZE_4>(std::forward<U>(args)...); } else { std::unreachable(); };
        case SIZE_8.ordinal(): if constexpr (((target_sizes == SIZE_8) || ...)) { return std::forward<Visitor>(visitor).template operator()<SIZE_8>(std::forward<U>(args)...); } else { std::unreachable(); };
        case SIZE_0.ordinal(): if constexpr (((target_sizes == SIZE_0) || ...)) { return std::forward<Visitor>(visitor).template operator()<SIZE_0>(std::forward<U>(args)...); } else { std::unreachable(); };
        default: std::unreachable();
    }
}
