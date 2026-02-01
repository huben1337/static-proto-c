#pragma once

namespace estd {

    struct empty {
        consteval empty () = default;
        consteval empty (const empty& /*unused*/) = default;
        constexpr empty (empty&& /*unused*/) = default;

        consteval empty& operator = (const empty&) = default;
        consteval empty& operator = (empty&&) = default;

        constexpr ~empty () = default;

        consteval empty operator*() const noexcept { return {}; }
        // consteval const empty* operator->() const noexcept { return this; }

        // comparison - disabled since i can't find a sensible way to do this
        // consteval bool operator==(empty /*unused*/) const { return true; }
        // consteval bool operator!=(empty /*unused*/) const { return false; }
        // consteval bool operator< (empty /*unused*/) const { return false; }
        // consteval bool operator<=(empty /*unused*/) const { return true; }
        // consteval bool operator> (empty /*unused*/) const { return false; }
        // consteval bool operator>=(empty /*unused*/) const { return true; }

        // unary
        consteval empty operator+() const { return {}; }
        consteval empty operator-() const { return {}; }

        // inc / dec
        consteval empty operator++()    const { return {}; }
        consteval empty operator++(int) const { return {}; }
        consteval empty operator--()    const { return {}; }
        consteval empty operator--(int) const { return {}; }

        // arithmetic
        consteval empty operator+(empty /*unused*/) const { return {}; }
        consteval empty operator-(empty /*unused*/) const { return {}; }
        consteval empty operator*(empty /*unused*/) const { return {}; }
        consteval empty operator/(empty /*unused*/) const { return {}; }
        consteval empty operator%(empty /*unused*/) const { return {}; }

        // bitwise
        consteval empty operator &(empty /*unused*/) const { return {}; }
        consteval empty operator |(empty /*unused*/) const { return {}; }
        consteval empty operator ^(empty /*unused*/) const { return {}; }
        consteval empty operator<<(empty /*unused*/) const { return {}; }
        consteval empty operator>>(empty /*unused*/) const { return {}; }

        // compound assignment
        consteval empty operator +=(empty /*unused*/) const { return {}; }
        consteval empty operator -=(empty /*unused*/) const { return {}; }
        consteval empty operator *=(empty /*unused*/) const { return {}; }
        consteval empty operator /=(empty /*unused*/) const { return {}; }
        consteval empty operator %=(empty /*unused*/) const { return {}; }
        consteval empty operator &=(empty /*unused*/) const { return {}; }
        consteval empty operator |=(empty /*unused*/) const { return {}; }
        consteval empty operator ^=(empty /*unused*/) const { return {}; }
        consteval empty operator<<=(empty /*unused*/) const { return {}; }
        consteval empty operator>>=(empty /*unused*/) const { return {}; }
    };
}