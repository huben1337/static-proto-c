#pragma once

#include <compare>
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

    [[deprecated("Internal field.")]] value_t _value;

protected:
    constexpr explicit ENUM_CLASS (const value_t value) : _value(value) {}

public:
    [[nodiscard]] constexpr bool operator == (this const outside_t& self, const outside_t& other) {
        return self.ordinal() == other.ordinal();
    }

    [[nodiscard]] constexpr const value_t& ordinal () const {
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wdeprecated-declarations"
        return _value;
        #pragma clang diagnostic pop
    }

    operator bool () const = delete;
    operator bool () = delete;

    template <typename writer_params>
    void log (const logger::writer<writer_params> w) const {
        w.template write<true, true>(outside_name + "{value: "_sl, ordinal(), "}");
    }
};

}