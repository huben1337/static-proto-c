#pragma once

namespace estd {

template <typename T>
struct ENUM_CLASS {
public:
    using value_t = T;

    value_t value;

    constexpr ENUM_CLASS () = default;
    constexpr ENUM_CLASS (const ENUM_CLASS&) = default;
    constexpr ENUM_CLASS (ENUM_CLASS&&) = default;

    constexpr ENUM_CLASS& operator = (const ENUM_CLASS&) = default;
    constexpr ENUM_CLASS& operator = (ENUM_CLASS&&) = default;

    // NOLINTNEXTLINE(google-explicit-constructor)
    [[nodiscard]] constexpr operator value_t () const { return value; }

    [[nodiscard]] explicit operator bool () const = delete;
protected:
    constexpr explicit ENUM_CLASS (const value_t value) : value(value) {}
};

}