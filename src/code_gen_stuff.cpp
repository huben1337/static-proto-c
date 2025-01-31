#pragma once

#include "base.cpp"
#include "constexpr_helpers.cpp"

constexpr size_t log10 (size_t value) {
    return value < 10 ? 0 : 1 + log10(value / 10);
}

template <size_t value>
constexpr auto uint_to_string () {
    constexpr size_t length = log10(value) + 1;
    char data[length + 1];
    size_t v = value;
    for (size_t i = 0; i < length; i++) {
        data[length - i - 1] = '0' + (v % 10);
        v /= 10;
    }
    data[length] = '\0';
    return StringLiteral(data);
}

template <size_t N>
constexpr size_t generated_array_ctors_size () {
    size_t ctor_args_size = 0;
    size_t ctor_inits_size = 0;
    size_t ctor_used_size = 0;
    size_t el_ctor_used_size = 0;
    for_([&]<size_t i>() {
        ctor_args_size += StringLiteral("size_t base").size();
        ctor_inits_size += StringLiteral("base(base)").size();
        ctor_used_size += StringLiteral("return {base").size();
        el_ctor_used_size += StringLiteral("return {base").size();

        for_([&]<size_t j>() {
            size_t idx_str_size = StringLiteral("idx_").size() + uint_to_string<j>().size();
            ctor_args_size += StringLiteral(", uint32_t ").size() + idx_str_size;
            ctor_inits_size += StringLiteral(", ").size() + idx_str_size + StringLiteral("(").size() + idx_str_size + StringLiteral(")").size();
            ctor_used_size += StringLiteral(", ").size() + idx_str_size;
            if constexpr (j == i - 1) {
                el_ctor_used_size += StringLiteral(", idx").size();
            } else {
                el_ctor_used_size += StringLiteral(", ").size() + idx_str_size;
            }
        }, std::make_integer_sequence<size_t, i>());

        ctor_used_size += StringLiteral("};").size();
        el_ctor_used_size += StringLiteral("};").size();
    }, std::make_integer_sequence<size_t, N>());

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

template <size_t N>
struct GeneratedArrayCtorStrs {
    std::array<char, generated_array_ctors_size<N>()> data;
    std::array<ArrayCtorStrIdxs, N> strs_array;
};


template <size_t N, size_t M>
constexpr size_t copy_str (StringLiteral<N> str, char (&out)[M], size_t pos) {
    std::copy_n(str.value.begin(), N - 1, out + pos);
    return pos + N - 1;
}

template <size_t N, size_t M>
constexpr size_t copy_str (const char (&str)[N], char (&out)[M], size_t pos) {
    std::copy_n(str, N - 1, out + pos);
    return pos + N - 1;
}


constexpr size_t array_ctor_strs_count = 70;

template <size_t N>
constexpr GeneratedArrayCtorStrs<N> _make_g_array_ctor_strs () {
    char data[generated_array_ctors_size<N>()];
    std::array<ArrayCtorStrIdxs, N> strs_array;
    
    size_t data_pos = 0;
    for_([&]<size_t i>() {

        size_t ctor_args_start = data_pos;
        data_pos = copy_str("size_t base", data, data_pos);
        for_([&]<size_t j>() {
            auto idx_str = StringLiteral("idx_") + uint_to_string<j>();
            data_pos = copy_str(StringLiteral(", uint32_t ") + idx_str, data, data_pos);
        }, std::make_integer_sequence<size_t, i>());
        Section ctor_args_section = {ctor_args_start, data_pos - ctor_args_start};

        size_t ctor_inits_start = data_pos;
        data_pos = copy_str("base(base)", data, data_pos);
        for_([&]<size_t j>() {
            auto idx_str = StringLiteral("idx_") + uint_to_string<j>();
            data_pos = copy_str(StringLiteral(", ") + idx_str + StringLiteral("(") + idx_str + StringLiteral(")"), data, data_pos);
        }, std::make_integer_sequence<size_t, i>());
        Section ctor_inits_section = {ctor_inits_start, data_pos - ctor_inits_start};

        size_t ctor_used_start = data_pos;
        data_pos = copy_str("return {base", data, data_pos);
        for_([&]<size_t j>() {
            auto idx_str = StringLiteral("idx_") + uint_to_string<j>();
            data_pos = copy_str(StringLiteral(", ") + idx_str, data, data_pos);
        }, std::make_integer_sequence<size_t, i>());
        data_pos = copy_str("};", data, data_pos);
        Section ctor_used_section = {ctor_used_start, data_pos - ctor_used_start};

        size_t el_ctor_used_start = data_pos;
        data_pos = copy_str("return {base", data, data_pos);
        if constexpr (i > 0) {
            for_([&]<size_t j>() {
                auto idx_str = StringLiteral("idx_") + uint_to_string<j>();
                data_pos = copy_str(StringLiteral(", ") + idx_str, data, data_pos);
            }, std::make_integer_sequence<size_t, i - 1>());
            data_pos = copy_str(", idx", data, data_pos);
        }
        data_pos = copy_str("};", data, data_pos);
        Section el_ctor_used_section = {el_ctor_used_start, data_pos - el_ctor_used_start};

        strs_array[i] = {ctor_args_section, ctor_inits_section, ctor_used_section, el_ctor_used_section};
    }, std::make_integer_sequence<size_t, N>());

    return {to_array(data), strs_array};
}
constexpr auto g_array_ctor_strs = _make_g_array_ctor_strs<array_ctor_strs_count>();