#pragma once

namespace estd {

    struct no_move_ctr {
        constexpr no_move_ctr() = default;
        no_move_ctr (no_move_ctr&&) = delete;
    };

    struct no_copy_ctr {
        constexpr no_copy_ctr() = default;
        no_copy_ctr (const no_copy_ctr&) = delete;
    };

    struct no_move_assign {
        constexpr no_move_assign() = default;
        no_move_assign& operator = (no_move_assign&&) = delete;
    };

    struct no_copy_assign {
        constexpr no_copy_assign() = default;
        no_copy_assign& operator = (const no_copy_assign&) = delete;
    };

    struct no_cpmv_ctr : no_move_ctr, no_copy_ctr {};

    struct no_cpmv_assign : no_move_assign, no_copy_assign {};

    struct unique_only : no_cpmv_ctr, no_cpmv_assign {};
}