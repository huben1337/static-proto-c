#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <string_view>
#include "./util/string_literal.hpp"
#include "./helper/ce.hpp"

namespace code_generation_static_data {

namespace detail {

template <typename T>
struct ArrayCtorStrsBase {
    T ctor_args;
    T ctor_inits;
    T ctor_used;
    T el_ctor_used;
};

}

struct ArrayCtorStrs : detail::ArrayCtorStrsBase<std::string_view> {
    [[nodiscard]] static constexpr ArrayCtorStrs make (uint8_t array_depth);
};


namespace detail {


struct StrSection {
    size_t offset;
    size_t length;

    [[nodiscard]] constexpr std::string_view access (const char* const data) const {
        return {data + offset, length};
    }
};


struct ArrayCtorStrSections : ArrayCtorStrsBase<StrSection> {

    [[nodiscard]] constexpr ArrayCtorStrs access (const char* const data) const {
        return {
            ctor_args.access(data),
            ctor_inits.access(data),
            ctor_used.access(data),
            el_ctor_used.access(data)
        };
    }
};

template <size_t N>
struct ArrayCtorStrsData {
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

    std::array<ArrayCtorStrSections, N> strs_sections;
    std::array<char, strs_data_size()> strs_data;

    [[nodiscard]] constexpr ArrayCtorStrs get (const uint8_t i) const {
        return strs_sections[i].access(strs_data.data());
    }
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
consteval auto make_array_ctor_strs () {
    // std::array<char, > data {};
    // std::array<ArrayCtorStrSections, N> strs_array {};
    ArrayCtorStrsData<N> data {};

    size_t data_pos = 0;
    ce::for_([&]<size_t i>() {

        const size_t ctor_args_start = data_pos;
        data_pos = copy_str("size_t base", data.strs_data, data_pos);
        ce::for_([&]<size_t j>() {
            data_pos = copy_str(", uint32_t idx_", data.strs_data, data_pos);
            data_pos = copy_str(string_literal::from<j>, data.strs_data, data_pos);
        }, std::make_index_sequence<i>{});
        const StrSection ctor_args_section {ctor_args_start, data_pos - ctor_args_start};

        size_t ctor_inits_start = data_pos;
        data_pos = copy_str("base(base)", data.strs_data, data_pos);
        ce::for_([&]<size_t j>() {
            constexpr auto idx_str = string_literal::from<j>;
            data_pos = copy_str(", idx_", data.strs_data, data_pos);
            data_pos = copy_str(idx_str, data.strs_data, data_pos);
            data_pos = copy_str("(idx_", data.strs_data, data_pos);
            data_pos = copy_str(idx_str, data.strs_data, data_pos);
            data_pos = copy_str(")", data.strs_data, data_pos);
        }, std::make_index_sequence<i>{});
        const StrSection ctor_inits_section {ctor_inits_start, data_pos - ctor_inits_start};

        const size_t ctor_used_start = data_pos;
        if constexpr (i > 0) {
            data_pos = copy_str("return {base", data.strs_data, data_pos);
            ce::for_([&]<size_t j>() {
                data_pos = copy_str(", idx_", data.strs_data, data_pos);
                data_pos = copy_str(string_literal::from<j>, data.strs_data, data_pos);
            }, std::make_index_sequence<i>{});
            data_pos = copy_str("};", data.strs_data, data_pos);
        } else {
            data_pos = copy_str("return {base};", data.strs_data, data_pos);
        }
        const StrSection ctor_used_section {ctor_used_start, data_pos - ctor_used_start};

        const size_t el_ctor_used_start = data_pos;
        if constexpr (i > 0) {
            data_pos = copy_str("return {base", data.strs_data, data_pos);
            ce::for_([&]<size_t j>() {
                data_pos = copy_str(", idx_", data.strs_data, data_pos);
                data_pos = copy_str(string_literal::from<j>, data.strs_data, data_pos);
            }, std::make_index_sequence<i - 1>{});
            data_pos = copy_str(", idx};", data.strs_data, data_pos);
        } else {
            data_pos = copy_str("return {base};", data.strs_data, data_pos);
        }
        const StrSection el_ctor_used_section {el_ctor_used_start, data_pos - el_ctor_used_start};

        data.strs_sections[i] = {ctor_args_section, ctor_inits_section, ctor_used_section, el_ctor_used_section};
    }, std::make_index_sequence<N>{});

    return data;
}

} // namespace detail



[[nodiscard]] constexpr ArrayCtorStrs ArrayCtorStrs::make (const uint8_t array_depth) {
    static constexpr size_t array_ctor_strs_count = 64;
    static constexpr auto array_ctor_strs = detail::make_array_ctor_strs<array_ctor_strs_count>();

    return array_ctor_strs.get(array_depth);
}

} // namespace code_generation_static_data
