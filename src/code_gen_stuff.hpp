#pragma once

#include <array>
#include <algorithm>
#include "./util/string_literal.hpp"
#include "./helper/ce.hpp"


namespace {

    template <size_t N>
    consteval size_t generated_array_ctors_size () {
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

    struct Section {
        size_t start;
        size_t length;
    };
    struct ArrayCtorStrIdxs {
        Section ctor_args;
        Section ctor_inits;
        Section ctor_used;
        Section el_ctor_used;
    };

    template <size_t N , size_t M>
    struct GeneratedArrayCtorStrs {
        std::array<ArrayCtorStrIdxs, N> strs_array;
        std::array<char, M> data;
    };


    template <size_t N, size_t M>
    consteval size_t copy_str (StringLiteral<N> str, std::array<char, M>& out, size_t pos) {
        std::copy_n(str.data, N - 1, out.begin() + pos);
        return pos + N - 1;
    }

    template <size_t N, size_t M>
    consteval size_t copy_str (const char (&str)[N], std::array<char, M>& out, size_t pos) {
        std::copy_n(str, N - 1, out.begin() + pos);
        return pos + N - 1;
    }

    template <size_t N>
    consteval auto make_g_array_ctor_strs () {
        std::array<char, generated_array_ctors_size<N>()> data {};
        std::array<ArrayCtorStrIdxs, N> strs_array {};

        size_t data_pos = 0;
        ce::for_([&]<size_t i>() {

            const size_t ctor_args_start = data_pos;
            data_pos = copy_str("size_t base", data, data_pos);
            ce::for_([&]<size_t j>() {
                data_pos = copy_str(", uint32_t idx_", data, data_pos);
                data_pos = copy_str(string_literal::from<j>, data, data_pos);
            }, std::make_index_sequence<i>{});
            const Section ctor_args_section = {ctor_args_start, data_pos - ctor_args_start};

            size_t ctor_inits_start = data_pos;
            data_pos = copy_str("base(base)", data, data_pos);
            ce::for_([&]<size_t j>() {
                constexpr auto idx_str = string_literal::from<j>;
                data_pos = copy_str(", idx_", data, data_pos);
                data_pos = copy_str(idx_str, data, data_pos);
                data_pos = copy_str("(idx_", data, data_pos);
                data_pos = copy_str(idx_str, data, data_pos);
                data_pos = copy_str(")", data, data_pos);
            }, std::make_index_sequence<i>{});
            const Section ctor_inits_section = {ctor_inits_start, data_pos - ctor_inits_start};

            const size_t ctor_used_start = data_pos;
            if constexpr (i > 0) {
                data_pos = copy_str("return {base", data, data_pos);
                ce::for_([&]<size_t j>() {
                    data_pos = copy_str(", idx_", data, data_pos);
                    data_pos = copy_str(string_literal::from<j>, data, data_pos);
                }, std::make_index_sequence<i>{});
                data_pos = copy_str("};", data, data_pos);
            } else {
                data_pos = copy_str("return {base};", data, data_pos);
            }
            const Section ctor_used_section = {ctor_used_start, data_pos - ctor_used_start};

            const size_t el_ctor_used_start = data_pos;
            if constexpr (i > 0) {
                data_pos = copy_str("return {base", data, data_pos);
                ce::for_([&]<size_t j>() {
                    data_pos = copy_str(", idx_", data, data_pos);
                    data_pos = copy_str(string_literal::from<j>, data, data_pos);
                }, std::make_index_sequence<i - 1>{});
                data_pos = copy_str(", idx};", data, data_pos);
            } else {
                data_pos = copy_str("return {base};", data, data_pos);
            }

            const Section el_ctor_used_section = {el_ctor_used_start, data_pos - el_ctor_used_start};

            strs_array[i] = {ctor_args_section, ctor_inits_section, ctor_used_section, el_ctor_used_section};
        }, std::make_index_sequence<N>{});

        return GeneratedArrayCtorStrs{strs_array, data};
    }

}

constexpr size_t array_ctor_strs_count = 64;

constexpr auto g_array_ctor_strs = make_g_array_ctor_strs<array_ctor_strs_count>();
