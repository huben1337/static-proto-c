#pragma once

#include <concepts>

#include "../util/string_literal.hpp"
#include "../util/logger.hpp"

namespace estd {

namespace {

    template <typename Outside>
    constexpr StringLiteral enum_class_outside_name = string_literal::from_([](){ return nameof::nameof_type<Outside>(); });

    template <>
    constexpr StringLiteral enum_class_outside_name<void> = "ENUM_CLASS<>";
}

template <std::integral T, typename Outside = void>
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

    template <typename writer_params>
    void log (logger::writer<writer_params> w) const {
        w.template write<true, true>(enum_class_outside_name<Outside> + "{value: "_sl, value, "}");
    }

protected:
    constexpr explicit ENUM_CLASS (const value_t value) : value(value) {}
    constexpr ~ENUM_CLASS () = default;
};

}