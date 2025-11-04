#pragma once

namespace estd {

    struct empty {
        consteval empty () = default;
        consteval empty (const empty& /*unused*/) = default;
        constexpr empty (empty&& /*unused*/)  noexcept {}

        empty& operator = (const empty&) = delete;
        empty& operator = (empty&&) = delete;

        constexpr ~empty () = default;
    };
}