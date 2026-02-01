#pragma once

#include <concepts>

#include "../util/string_literal.hpp"
#include "../util/logger.hpp"

namespace estd {

template <std::integral T, typename Outside = void>
struct ENUM_CLASS {
private:

    template <typename Outside_>
    struct outside {
        using type = Outside;
    };

    template <>
    struct outside<void> {
        using type = ENUM_CLASS;
    };

    template <typename Outside_>
    static constexpr StringLiteral outside_name_v = string_literal::from_([](){ return nameof::nameof_type<Outside>(); });

    template <>
    constexpr StringLiteral outside_name_v<void> = "ENUM_CLASS<>";

public:
    using value_t = T;
    using outside_t = outside<Outside>::type;
    static constexpr StringLiteral outside_name = outside_name_v<Outside>;

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
    void log (const logger::writer<writer_params> w) const {
        w.template write<true, true>(outside_name + "{value: "_sl, value, "}");
    }

protected:
    constexpr explicit ENUM_CLASS (const value_t value) : value(value) {}
    constexpr ~ENUM_CLASS () = default;
};

}