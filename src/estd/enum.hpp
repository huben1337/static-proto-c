#pragma once

#include <concepts>
#include <type_traits>

#include "../estd/visit.hpp"
#include "../util/string_literal.hpp"
#include "../util/logger.hpp"

namespace estd {

template <std::integral T, typename Derived>
struct enum_ {
    using value_t = T;
    static constexpr StringLiteral type_name = string_literal::from_([](){ return nameof::nameof_type<Derived>(); });

    [[deprecated("Internal field.")]] value_t _value;

protected:
    constexpr explicit enum_ (const value_t value) : _value(value) {
        static_assert(std::is_base_of_v<enum_, Derived>, "Derived must inherit from enum_<T, Derived>");
    }

public:
    operator bool () const = delete;
    operator bool () = delete;

    [[nodiscard]] constexpr bool operator == (this const Derived& self, const Derived& other) {
        return self.ordinal() == other.ordinal();
    }

    [[nodiscard]] constexpr const value_t& ordinal () const {
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wdeprecated-declarations"
        return _value;
        #pragma clang diagnostic pop
    }

    template <typename Result, auto... variants, typename... U, typename Visitor>
    [[nodiscard]] constexpr Result visit (this const Derived& self, estd::variadic_v<variants...> /*unused*/, Visitor&& visitor, U&&... args) {
        static_assert((std::is_same_v<decltype(variants), Derived> && ...));
        return estd::visit<Result>(self, estd::variadic_v<variants...>{}, std::forward<Visitor>(visitor), {}, std::forward<U>(args)...);
    }

    template <typename writer_params>
    void log (const logger::writer<writer_params> w) const {
        w.template write<true, true>(type_name + "{value: "_sl, ordinal(), "}");
    }
};

}