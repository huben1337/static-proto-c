#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <string_view>
#include "./util/string_literal.hpp"
#include "./helper/ce.hpp"

namespace code_generation_static_data {

struct ArrayCtorStrs {
    std::string_view ctor_args;
    std::string_view ctor_inits;
    std::string_view ctor_used;
    std::string_view el_ctor_used;

    [[nodiscard]] static constexpr ArrayCtorStrs make (uint8_t array_depth);
};

namespace detail {

template <size_t N>
struct ArrayCtorStrsData {
    friend ArrayCtorStrs;
private:
    [[nodiscard]] static consteval size_t strs_data_size () {
        size_t ctor_args_size = 0;
        size_t ctor_inits_size = 0;
        size_t ctor_used_size = 0;
        size_t el_ctor_used_size = 0;
        ce::for_([&]<size_t i>() {
            ctor_args_size += "size_t base"_sl.size();
            ctor_inits_size += "base(base)"_sl.size();
            ctor_used_size += "return {base"_sl.size();
            el_ctor_used_size += "return {base"_sl.size();

            ce::for_([&]<size_t j>() {
                constexpr size_t idx_str_size = "idx_"_sl.size() + string_literal::from<j>.size();
                ctor_args_size += ", uint32_t "_sl.size() + idx_str_size;
                ctor_inits_size += ", "_sl.size() + idx_str_size + "("_sl.size() + idx_str_size + ")"_sl.size();
                ctor_used_size += ", "_sl.size() + idx_str_size;
                if constexpr (j == i - 1) {
                    el_ctor_used_size += ", idx"_sl.size();
                } else {
                    el_ctor_used_size += ", "_sl.size() + idx_str_size;
                }
            }, std::make_index_sequence<i>{});

            ctor_used_size += "};"_sl.size();
            el_ctor_used_size += "};"_sl.size();
        }, std::make_index_sequence<N>{});

        return ctor_args_size + ctor_inits_size + ctor_used_size + el_ctor_used_size;
    }

    std::array<ArrayCtorStrs, N> strs_views;
    std::array<char, strs_data_size()> strs_data;

    template <size_t M>
    [[nodiscard]] static consteval char* copy_str (const StringLiteral<M>& str, char* const pos) {
        return std::copy_n(str.data, M, pos);
    }

    template <size_t M>
    [[nodiscard]] static consteval char* copy_str (const char (&str)[M], char* const pos) {
        return std::copy_n(str, M - 1, pos);
    }

    consteval ArrayCtorStrsData () {
        char* data_pos = strs_data.data();
        ce::for_([&]<size_t i>() {

            char* const ctor_args_start = data_pos;
            data_pos = copy_str("size_t base", data_pos);
            ce::for_([&]<size_t j>() {
                data_pos = copy_str(", uint32_t idx_", data_pos);
                data_pos = copy_str(string_literal::from<j>, data_pos);
            }, std::make_index_sequence<i>{});
            const std::string_view ctor_args_str {ctor_args_start, data_pos};

            char* const ctor_inits_start = data_pos;
            data_pos = copy_str("base(base)", data_pos);
            ce::for_([&]<size_t j>() {
                constexpr auto idx_str = string_literal::from<j>;
                data_pos = copy_str(", idx_", data_pos);
                data_pos = copy_str(idx_str, data_pos);
                data_pos = copy_str("(idx_", data_pos);
                data_pos = copy_str(idx_str, data_pos);
                data_pos = copy_str(")", data_pos);
            }, std::make_index_sequence<i>{});
            const std::string_view ctor_inits_str {ctor_inits_start, data_pos};

            char* const ctor_used_start = data_pos;
            if constexpr (i > 0) {
                data_pos = copy_str("return {base", data_pos);
                ce::for_([&]<size_t j>() {
                    data_pos = copy_str(", idx_", data_pos);
                    data_pos = copy_str(string_literal::from<j>, data_pos);
                }, std::make_index_sequence<i>{});
                data_pos = copy_str("};", data_pos);
            } else {
                data_pos = copy_str("return {base};", data_pos);
            }
            const std::string_view ctor_used_str {ctor_used_start, data_pos};

            char* const el_ctor_used_start = data_pos;
            if constexpr (i > 0) {
                data_pos = copy_str("return {base", data_pos);
                ce::for_([&]<size_t j>() {
                    data_pos = copy_str(", idx_", data_pos);
                    data_pos = copy_str(string_literal::from<j>, data_pos);
                }, std::make_index_sequence<i - 1>{});
                data_pos = copy_str(", idx};", data_pos);
            } else {
                data_pos = copy_str("return {base};", data_pos);
            }
            const std::string_view el_ctor_used_str {el_ctor_used_start, data_pos};

            strs_views[i] = {ctor_args_str, ctor_inits_str, ctor_used_str, el_ctor_used_str};
        }, std::make_index_sequence<N>{});
    }
};

} // namespace detail



[[nodiscard]] constexpr ArrayCtorStrs ArrayCtorStrs::make (const uint8_t array_depth) {
    static constexpr size_t array_ctor_strs_count = 64;
    static constexpr auto array_ctor_strs = detail::ArrayCtorStrsData<array_ctor_strs_count>{};

    return array_ctor_strs.strs_views[array_depth];
}

} // namespace code_generation_static_data
