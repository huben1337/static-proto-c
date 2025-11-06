#pragma once

namespace estd {

    struct no_move_ctr {
        constexpr no_move_ctr() = default;
        constexpr no_move_ctr(const no_move_ctr&) = default;
        constexpr no_move_ctr(no_move_ctr&&) = delete;

        constexpr no_move_ctr& operator=(const no_move_ctr&) = default;
        constexpr no_move_ctr& operator=(no_move_ctr&&) = default;

        constexpr ~no_move_ctr() = default;
    };

    struct no_copy_ctr {
        constexpr no_copy_ctr() = default;
        constexpr no_copy_ctr(const no_copy_ctr&) = delete;
        constexpr no_copy_ctr(no_copy_ctr&&) = default;

        constexpr no_copy_ctr& operator=(const no_copy_ctr&) = default;
        constexpr no_copy_ctr& operator=(no_copy_ctr&&) = default;

        constexpr ~no_copy_ctr() = default;
    };

    struct no_move_assign {
        constexpr no_move_assign() = default;
        constexpr no_move_assign(const no_move_assign&) = default;
        constexpr no_move_assign(no_move_assign&&) = default;

        constexpr no_move_assign& operator=(const no_move_assign&) = default;
        constexpr no_move_assign& operator=(no_move_assign&&) = delete;

        constexpr ~no_move_assign() = default;
    };

    struct no_copy_assign {
        constexpr no_copy_assign() = default;
        constexpr no_copy_assign(const no_copy_assign&) = default;
        constexpr no_copy_assign(no_copy_assign&&) = default;

        constexpr no_copy_assign& operator=(const no_copy_assign&) = delete;
        constexpr no_copy_assign& operator=(no_copy_assign&&) = default;

        constexpr ~no_copy_assign() = default;
    };

    struct no_cpmv_ctr : no_move_ctr, no_copy_ctr {};

    struct no_cpmv_assign : no_move_assign, no_copy_assign {};

    struct no_copy : no_copy_ctr, no_copy_assign {};

    struct no_move : no_move_ctr, no_move_assign {};

    struct unique_only : no_cpmv_ctr, no_cpmv_assign {};
}