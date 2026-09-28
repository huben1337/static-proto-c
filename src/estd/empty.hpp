#pragma once

namespace estd {

    struct empty {
        consteval empty () = default;
        consteval empty (const empty& /*unused*/) = default;
        constexpr empty (empty&& /*unused*/) = default;

        consteval empty& operator = (const empty&) = default;
        consteval empty& operator = (empty&&) = default;

        constexpr ~empty () = default;
    };
}