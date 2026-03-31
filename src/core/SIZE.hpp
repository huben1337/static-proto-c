#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <gsl/util>
#include <type_traits>

#include "../estd/utility.hpp"
#include "../estd/enum.hpp"


struct SIZE : estd::enum_<uint8_t, SIZE> {

    using enum_::enum_;

    template <value_t>
    struct Mapped;
    
    static const SIZE SIZE_1;
    static const SIZE SIZE_2;
    static const SIZE SIZE_4;
    static const SIZE SIZE_8;
    static const SIZE SIZE_0;

    static const SIZE MIN;
    static const SIZE MAX;

    constexpr SIZE () : SIZE{SIZE_0} {}

    [[nodiscard]] constexpr std::strong_ordering operator <=> (this const SIZE& self, const SIZE& other) {
        return self.ordinal() <=> other.ordinal();
    }
private:
    static void substraction_invalid () {}

    [[nodiscard]] constexpr bool can_substract (this const SIZE& self, const SIZE& other) {
        return
            self != SIZE_0 &&
            other != SIZE_0 &&
            (static_cast<int>(self.ordinal()) - static_cast<int>(other.ordinal())) >= MIN.ordinal();
    }

public:
    [[nodiscard]] constexpr SIZE operator - (this const SIZE& self, const SIZE& other) {
        if consteval {
            if (self.can_substract(other)) {
                substraction_invalid();
            }
        } else {
            BSSERT(self.can_substract(other));
        }
        return SIZE{gsl::narrow_cast<value_t>(self.ordinal() - other.ordinal())};
    }

private:
    static void addition_invalid () {}

    [[nodiscard]] constexpr bool can_add (this const SIZE& self, const SIZE& other) {
        return
            self != SIZE_0 &&
            other != SIZE_0 &&
            (static_cast<int>(self.ordinal()) + static_cast<int>(other.ordinal())) <= MAX.ordinal();
    }

public:
    [[nodiscard]] constexpr SIZE operator + (this const SIZE& self, const SIZE& other) {
        if consteval {
            if (self.can_add(other)) {
                addition_invalid();
            }
        } else {
            BSSERT(self.can_add(other));
        }
        return SIZE{gsl::narrow_cast<value_t>(self.ordinal() + other.ordinal())};
    }

    [[nodiscard]] constexpr value_t byte_size () const {
        return gsl::narrow_cast<value_t>(1 << ordinal());
    }

private:
    static void next_smaller_is_invalid_for_this_size () {}
    static void next_bigger_is_invalid_for_this_size () {}

public:
    [[nodiscard]] consteval SIZE next_smaller (this const SIZE& self) {
        if (self == SIZE_0 || self == SIZE_1) {
            next_smaller_is_invalid_for_this_size();
        }
        return SIZE{gsl::narrow_cast<value_t>(self.ordinal() - 1)};
    }

    [[nodiscard]] consteval SIZE next_bigger (this const SIZE& self) {
        if (self == SIZE_0 || self == SIZE_8) {
            next_bigger_is_invalid_for_this_size();
        }
        return SIZE{gsl::narrow_cast<value_t>(self.ordinal() + 1)};
    }

    template <std::unsigned_integral T>
    [[nodiscard]] static SIZE from_integral (const T i) {
        return SIZE{gsl::narrow_cast<value_t>(i % gsl::narrow_cast<value_t>(MAX.ordinal() + 1))};
    }

    template <typename writer_params>
    void log (const logger::writer<writer_params> w) const {
        w.template write<true, true>(type_name + "_"_sl, byte_size());
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
    template <SIZE... w>
    using includes_value = std::bool_constant<((w == value) || ...)>;

    static_assert(
        value == SIZE::SIZE_0 ||
        SIZE::enums::apply<includes_value>::value
    );
};

