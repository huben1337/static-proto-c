#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <io.h>
#include <cstdio>
#include <span>
#include <string>
#include <array>
#include <string_view>
#include <type_traits>
#include <utility>
#include "base.cpp"
#include "generate_offsets.cpp"
#include "codegen.cpp"
#include "fatal_error.cpp"
#include "lexer_types.cpp"
#include "memory.cpp"
#include "string_helpers.cpp"
#include "string_literal.cpp"
#include "code_gen_stuff.cpp"
#include "fatal_error.cpp"
#include "logger.cpp"
namespace decode_code {

using generate_offsets::Offsets;

struct SizeLeaf {
    uint64_t min_size;
    uint16_t idx;
    lexer::SIZE size_size;
    lexer::SIZE stored_size_size;
};

struct OffsetsAccessor {
    OffsetsAccessor (
        const Offsets& offsets,
        const Buffer& var_offset_buffer,
        uint64_t var_leafs_start,
        uint16_t& current_map_idx,
        uint16_t& current_variant_field_idx
    ) :
    offsets(offsets),
    var_offset_buffer(var_offset_buffer),
    var_leafs_start(var_leafs_start),
    current_map_idx(current_map_idx),
    current_variant_field_idx(current_variant_field_idx)
    {}
    const Offsets offsets;
    const Buffer& var_offset_buffer;
    const uint64_t var_leafs_start;
    uint16_t& current_map_idx;
    uint16_t& current_variant_field_idx;

    INLINE uint16_t next_map_idx () const {
        uint16_t map_idx = current_map_idx++;
        logger::debug("next_map_idx: ", map_idx);
        return offsets.idx_map[map_idx];
    }

    INLINE uint16_t next_variant_field_idx () const {
        uint16_t variant_field_idx = current_variant_field_idx++;
        logger::debug("next_variant_field_idx: ", variant_field_idx);
        return variant_field_idx;
    }

    INLINE uint64_t next_fixed_offset () const {
        return offsets.fixed_offsets[next_map_idx()];
    }

    INLINE Buffer::View<uint64_t> next_var_offset () const {
        return offsets.var_offsets[next_map_idx()];
    }

    template<bool is_fixed>
    INLINE std::conditional_t<is_fixed, uint64_t, Buffer::View<uint64_t>> next_offset () const {
        if constexpr (is_fixed) {
            return next_fixed_offset();
        } else {
            return next_var_offset();
        }
    }
};

INLINE std::string get_size_type_str (lexer::SIZE size) {
    switch (size)
    {
    case lexer::SIZE::SIZE_1:
        return "uint8_t";
    case lexer::SIZE::SIZE_2:
        return "uint16_t";
    case lexer::SIZE::SIZE_4:
        return "uint32_t";
    case lexer::SIZE::SIZE_8:
        return "uint64_t";
    default:
        INTERNAL_ERROR("[get_size_type_str] invalid size");
    }
}

struct ArrayCtorStrs {
    std::string_view ctor_args;
    std::string_view ctor_inits;
    std::string_view ctor_used;
    std::string_view el_ctor_used;
};
/* Lookup table for the first 65 elements. (65 = 64 + 1. Since fixed arrays can only be of length 2 or more 64 nested is the max and 1 more for a outer dynamic array. If bools are packed perfectly add 3 more)*/
INLINE constexpr ArrayCtorStrs make_array_ctor_strs (uint8_t array_depth) {
    if (array_depth > array_ctor_strs_count) {
        INTERNAL_ERROR("[make_array_ctor_strs] array depth too large\n");
    }
    auto [ctor_args, ctor_inits, ctor_used, el_ctor_used] = g_array_ctor_strs.strs_array[array_depth];
    return ArrayCtorStrs{
        {g_array_ctor_strs.data.data() + ctor_args.start, ctor_args.length},
        {g_array_ctor_strs.data.data() + ctor_inits.start, ctor_inits.length},
        {g_array_ctor_strs.data.data() + ctor_used.start, ctor_used.length},
        {g_array_ctor_strs.data.data() + el_ctor_used.start, el_ctor_used.length}
    };
}

constexpr bool is_power_of_two(size_t n) {
    return (n != 0) && ((n & (n - 1)) == 0);
}

struct ArrayLengths {
    const uint32_t* const data;
    const uint8_t length;
};

struct SizeChainCodeGenerator : codegen::Generator {
    SizeChainCodeGenerator(
        const Buffer& var_offset_buffer,
        const Buffer::View<uint64_t>& size_chain
    ) :
    size_chain_data(var_offset_buffer.get_aligned(size_chain.start_idx)),
    size_chain_length(size_chain.length)
    {}

    SizeChainCodeGenerator(
        const uint64_t* const size_chain_data,
        const Buffer::index_t size_chain_length
    ) :
    size_chain_data(size_chain_data),
    size_chain_length(size_chain_length)
    {}

    const uint64_t* const size_chain_data;
    const Buffer::index_t size_chain_length;
    
    char* write(char* dst) const override {
        for (size_t i = 0; i < size_chain_length; i++) {
            const uint64_t size = size_chain_data[i];
            dst = codegen::write_string(dst, " + size"_sl);
            dst = codegen::write_string(dst, i);
            dst = codegen::write_string(dst, "(base)"_sl);
            if (size != 1) {
                dst = codegen::write_string(dst, " * "_sl);
                dst = codegen::write_string(dst, size);
            }
        }
        return dst;
    }

    size_t get_size() const override {
        size_t offset_str_size = size_chain_length * (" + size"_sl.size() + "(base)"_sl.size()) + fast_math::sum_of_digits_unsafe(size_chain_length);
        for (size_t i = 0; i < size_chain_length; i++) {
            const uint64_t size = size_chain_data[i];
            if (size != 1) {
                offset_str_size += " * "_sl.size() + fast_math::log10_unsafe(size) + 1;
            }
        }
        return offset_str_size;
    }
};

template <bool no_multiply, bool last_is_direct = false>
struct IdxCalcCodeGenerator : public codegen::OverAllocatedGenerator {
    IdxCalcCodeGenerator (const ArrayLengths& array_lengths) :
    array_lengths(array_lengths),
    estimated_size(estimate_size())
    {}

    const ArrayLengths& array_lengths;
    const size_t estimated_size;

    Buffer::index_t estimate_size () {
        if (array_lengths.length == 0) {
            INTERNAL_ERROR("[IdxCalcCodeGenerator::get_size] array_lengths.length must be > 0")
        } else if (array_lengths.length == 1) {
            if constexpr (last_is_direct) {
                return "idx"_sl.size();
            } else {
                return "idx_0"_sl.size();
            }
        } else {
            Buffer::index_t size;

            if constexpr (last_is_direct) {
                size = (array_lengths.length - 1) * (" + idx_"_sl.size() + " * "_sl.size() + 19) + "idx"_sl.size() + fast_math::sum_of_digits_unsafe(array_lengths.length - 1);
            } else {
                size = array_lengths.length * " + idx_"_sl.size() + (array_lengths.length - 1) * (" * "_sl.size() + 19) + fast_math::sum_of_digits_unsafe(array_lengths.length);
            }
    
            if constexpr (!no_multiply) {
                size += "("_sl.size() + ")"_sl.size();
            }

            return size;
        }
    }

    uint64_t get_idx_size_multiplier (uint8_t i) const {
        uint64_t size = 1;
        const uint32_t* const array_lengths_end = array_lengths.data + array_lengths.length - 1 - i;
        for (const uint32_t* array_length = array_lengths.data; array_length < array_lengths_end; array_length++) {
            size *= *array_length;
        }
        return size;
    }

    WriteResult write (char* dst) const override {
        char* const start = dst;
        if (array_lengths.length == 1) {
            if constexpr (last_is_direct) {
                dst = codegen::write_string(dst, "idx");
                goto done;
            } else {
                dst = codegen::write_string(dst, "idx_0");
                goto done;
            }
        } else {  
            if  constexpr (!no_multiply) {
                dst = codegen::write_string(dst, "(");
            }
    
            for (uint32_t i = 0; i < array_lengths.length - 1; i++) {
                dst = codegen::write_string(dst, " + idx_");
                dst = codegen::write_string(dst, i);
                dst = codegen::write_string(dst, " * ");
                dst = codegen::write_string(dst, get_idx_size_multiplier(i));
            }

            if constexpr (last_is_direct) {
                dst = codegen::write_string(dst, " + idx");
            } else {
                dst = codegen::write_string(dst, " + idx_");
                dst = codegen::write_string(dst, array_lengths.length - 1);
            }
    
            if constexpr (!no_multiply) {
                dst = codegen::write_string(dst, ")");
            }
        }
        done:;
        const size_t allocated_size = reinterpret_cast<size_t>(dst) - reinterpret_cast<size_t>(start);
        return {dst, static_cast<Buffer::index_t>(estimated_size - allocated_size)}; // This cast is allowed since estimated_size always fits into Buffer::index_t (This is checked when memory is requested from Buffer)
    }

    size_t get_size () const override {
        return estimated_size;
    }
};

struct GenStructFieldResult {
    lexer::StructField* next;
    codegen::__UnknownStruct code;
};

struct GenFixedArrayLeafArgs {
    uint16_t depth;
};

struct GenArrayLeafArgs {
};

struct GenFixedVariantLeafArgs {
    uint16_t variant_id;
    uint16_t variant_depth;
};

struct GenDynamicVariantLeafArgs : GenFixedVariantLeafArgs {
    std::string_view offset;
};

// template <bool in_array>
// struct GenFixedVariantLeafArgs : _GenVariantLeafArgs {};
// 
// template<>
// struct GenFixedVariantLeafArgs<true> : _GenVariantLeafArgs {
//     uint64_t max_size;
//     std::string idx_calc_str;
// };
// 
// template<>
// struct GenFixedVariantLeafArgs<false> : _GenVariantLeafArgs {};

struct GenStructLeafArgs {
    std::string_view name;
    uint16_t depth;
};

template<typename T>
concept LeafArgs = std::same_as<T, GenStructLeafArgs>
                || std::same_as<T, GenArrayLeafArgs>
                || std::same_as<T, GenFixedArrayLeafArgs>
                || std::same_as<T, GenFixedVariantLeafArgs>
                || std::same_as<T, GenDynamicVariantLeafArgs>;

template <LeafArgs ArgsT>
constexpr bool is_array_element = std::is_same_v<ArgsT, GenArrayLeafArgs> || std::is_same_v<ArgsT, GenFixedArrayLeafArgs>;

template <LeafArgs ArgsT>
constexpr bool is_variant_element = std::is_same_v<ArgsT, GenFixedVariantLeafArgs> || std::is_same_v<ArgsT, GenDynamicVariantLeafArgs>;

template <LeafArgs ArgsT>
constexpr bool is_dynamic_variant_element = std::is_same_v<ArgsT, GenDynamicVariantLeafArgs>;

template <LeafArgs ArgsT>
constexpr bool is_struct_element = std::is_same_v<ArgsT, GenStructLeafArgs>;

template <typename F, typename ... Args>
constexpr size_t arg_count( F(*f)(Args ...))
{
   return sizeof...(Args);
}

INLINE constexpr auto get_unique_name (const GenStructLeafArgs& additional_args) {
    return codegen::StringParts{additional_args.name, "_"_sl, additional_args.depth};
}
template <StringLiteral element_name>
INLINE constexpr auto get_unique_name (const GenStructLeafArgs& additional_args) {
    return codegen::StringParts{additional_args.name, "_"_sl, additional_args.depth};
}
template <typename F>
INLINE constexpr auto get_unique_name (const GenStructLeafArgs& additional_args, F on_element) {
    return codegen::StringParts{additional_args.name, "_"_sl, additional_args.depth};
}

template <StringLiteral element_name>
INLINE constexpr auto get_unique_name (const GenFixedArrayLeafArgs& additional_args) {
    return element_name;
}
template <typename F>
INLINE constexpr auto get_unique_name (const GenFixedArrayLeafArgs& additional_args, F on_element) {
    return on_element();
}

template <StringLiteral element_name>
INLINE constexpr auto get_unique_name (const GenArrayLeafArgs& additional_args) {
    return element_name;
}
template <typename F>
INLINE constexpr auto get_unique_name (const GenArrayLeafArgs& additional_args, F on_element) {
    return on_element();
}

INLINE constexpr auto get_unique_name (const GenFixedVariantLeafArgs& additional_args) {
    return codegen::StringParts{"_"_sl, additional_args.variant_depth, "_"_sl, additional_args.variant_id};
}
template <StringLiteral element_name>
INLINE constexpr auto get_unique_name (const GenFixedVariantLeafArgs& additional_args) {
    return codegen::StringParts{"_"_sl, additional_args.variant_depth, "_"_sl, additional_args.variant_id};
}
template <typename F>
INLINE constexpr auto get_unique_name (const GenFixedVariantLeafArgs& additional_args, F on_element) {
    return codegen::StringParts{"_"_sl, additional_args.variant_depth, "_"_sl, additional_args.variant_id};
}

// template <typename T>
// struct is_gen_variant_leaf_args : std::false_type {};
// template <bool in_array>
// struct is_gen_variant_leaf_args<GenFixedVariantLeafArgs<in_array>> : std::true_type {};

INLINE constexpr decltype(auto) get_name (const GenStructLeafArgs& additional_args) {
    return additional_args.name;
}
template <typename T>
INLINE constexpr auto get_name (const GenStructLeafArgs& additional_args, T) {
    return additional_args.name;
}

INLINE constexpr auto get_name (const GenFixedVariantLeafArgs& additional_args) {
    return codegen::StringParts{"as_"_sl, additional_args.variant_id};
}
template <typename T>
INLINE constexpr auto get_name (const GenFixedVariantLeafArgs& additional_args, T) {
    return codegen::StringParts{"as_"_sl, additional_args.variant_id};
}

template <LeafArgs ArgsT, typename T>
requires (!(std::is_same_v<ArgsT, GenStructLeafArgs> || std::is_same_v<ArgsT, GenFixedVariantLeafArgs> || std::is_same_v<ArgsT, GenDynamicVariantLeafArgs>))
INLINE constexpr auto get_name (ArgsT, T&& unique_name) {
    return std::forward<T>(unique_name);
}

template <typename NameT>
requires (!LeafArgs<std::remove_cvref_t<NameT>>)
INLINE constexpr auto get_name (NameT&& name) {
    return std::forward<NameT>(name);
}

template <bool condition, typename T, typename U>
INLINE constexpr std::conditional_t<condition, T, U> conditionally (T&& t, U&& u) {
    if constexpr (condition) {
        return std::forward<T>(t);
    } else {
        return std::forward<U>(u);
    }
}

template <typename CodeT>
INLINE CodeT add_size_leafs (
    std::span<SizeLeaf> level_size_leafs,
    uint64_t* fixed_offsets,
    CodeT&& struct_code
) {
    for (size_t i = 0; i < level_size_leafs.size(); i++) {
        auto [min_size, idx, size_size, stored_size_size] = level_size_leafs[i];
        uint64_t offset = fixed_offsets[idx];
        struct_code = struct_code
        .method(codegen::Attributes{"static"}, get_size_type_str(size_size), codegen::StringParts("size", i), codegen::Args{"size_t base"})
            .line("return *reinterpret_cast<", get_size_type_str(stored_size_size), "*>(base + ", offset, ");")
        .end();
    }
    return std::move(struct_code);
}

// template <typename Last>
// INLINE auto add_size_leafs (
//     std::span<SizeLeaf> level_size_leafs,
//     uint64_t* fixed_offsets,
//     codegen::NestedStruct<Last>&& struct_code
// ) {
//     return codegen::NestedStruct<Last>{add_size_leafs(
//         level_size_leafs,
//         fixed_offsets,
//         (codegen::__UnknownStruct{std::move(struct_code)})
//     )};
// }
// 
// template <typename Last>
// INLINE auto add_size_leafs (
//     std::span<SizeLeaf> level_size_leafs,
//     uint64_t* fixed_offsets,
//     codegen::Struct<Last>&& struct_code
// ) {
//     return codegen::Struct<Last>{add_size_leafs(
//         level_size_leafs,
//         fixed_offsets,
//         (codegen::__UnknownStruct{std::move(struct_code)})
//     )};
// }

constexpr lexer::SIZE delta_to_size (uint64_t delta) {
    if (delta <= UINT8_MAX) {
        return lexer::SIZE::SIZE_1;
    } else if (delta <= UINT16_MAX) {
        return lexer::SIZE::SIZE_2;
    } else if (delta <= UINT32_MAX) {
        return lexer::SIZE::SIZE_4;
    } else {
        return lexer::SIZE::SIZE_8;
    }
}

template <StringLiteral type_name, char target>
consteval auto last_non_whitespace_is () {
    size_t i = type_name.size();
    char last_char = type_name.value[--i];
    while (last_char == ' ' || last_char == '\t') {
        last_char = type_name.value[--i];
    }
    return last_char == target;
}

template <StringLiteral type_name>
consteval auto get_first_return_line_part () {
    if constexpr (last_non_whitespace_is<type_name, '*'>()) {
        return "return reinterpret_cast<"_sl + type_name + ">(base + "_sl;
    } else {
        return "return *reinterpret_cast<"_sl + type_name + "*>(base + "_sl;
    }
}


template <bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename Last>
INLINE codegen::Method<Last> _gen_fixed_value_leaf_in_array (
    codegen::Method<Last>&& get_method,
    OffsetsAccessor offsets_accessor,
    const ArrayLengths& array_lengths
) {
    constexpr auto type_size_str = uint_to_string<lexer::byte_size_of(type_size)>();

    uint64_t offset = offsets_accessor.next_fixed_offset();

    if constexpr (type_size == lexer::SIZE::SIZE_1) {
        if (offset == 0) {
            get_method = get_method
            .line(get_first_return_line_part<type_name>(), IdxCalcCodeGenerator<true, is_array_element>(array_lengths), ");");
        } else {
            get_method = get_method
            .line(get_first_return_line_part<type_name>(), offset, " + ", IdxCalcCodeGenerator<true, is_array_element>(array_lengths), ");");
        }
    } else {
        if (offset == 0) {
            get_method = get_method
            .line(get_first_return_line_part<type_name>(), IdxCalcCodeGenerator<false, is_array_element>(array_lengths), " * ", type_size_str, ");");
        } else {
            get_method = get_method
            .line(get_first_return_line_part<type_name>(), offset, " + ", IdxCalcCodeGenerator<false, is_array_element>(array_lengths), " * ", type_size_str, ");");
        }
    }

    return std::move(get_method);
}

template <bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename Last>
INLINE codegen::Method<Last> _gen_fixed_value_leaf_default (
    codegen::Method<Last>&& get_method,
    OffsetsAccessor offsets_accessor
) {
    uint64_t offset = offsets_accessor.next_fixed_offset();

    if (offset == 0) {
        get_method = get_method
        .line("return *reinterpret_cast<", type_name, "*>(base);");
    } else {
        get_method = get_method
        .line(get_first_return_line_part<type_name>(), offset, ");");
    }

    return std::move(get_method);
}

template <bool in_array, bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename ArgsT, typename CodeT>
INLINE CodeT _gen_fixed_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    ArgsT&& additional_args,
    const ArrayLengths& array_lengths
) {
    if constexpr (is_array_element) {
        auto&& get_method = code
        .method(type_name, "get", codegen::Args("uint32_t idx"));
        
        get_method = _gen_fixed_value_leaf_in_array<true, type_name, type_size>(std::move(get_method), offsets_accessor, array_lengths);

        code = get_method
        .end();
    } else {
        auto&& get_method = code
        .method(type_name, get_name(std::forward<ArgsT>(additional_args)));
        if constexpr (in_array) {
            get_method = _gen_fixed_value_leaf_in_array<false, type_name, type_size>(std::move(get_method), offsets_accessor, array_lengths);
        } else {
            get_method = _gen_fixed_value_leaf_default<false, type_name, type_size>(std::move(get_method), offsets_accessor);
        }
        code = get_method
        .end();
    }
    return std::move(code);
}

template <bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename Last>
INLINE codegen::Method<Last> _gen_var_value_leaf_in_array (
    codegen::Method<Last>&& get_method,
    OffsetsAccessor offsets_accessor,
    const ArrayLengths& array_lengths
) {
    constexpr auto type_size_str = uint_to_string<lexer::byte_size_of(type_size)>();

    uint64_t var_leafs_start = offsets_accessor.var_leafs_start;
    const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();

    if constexpr (type_size == lexer::SIZE::SIZE_1) {
        if (size_chain.empty()) {
            get_method = get_method
            .line(get_first_return_line_part<type_name>(), var_leafs_start, " + ", IdxCalcCodeGenerator<true, is_array_element>(array_lengths), ");");
        } else {
            get_method = get_method
            .line(get_first_return_line_part<type_name>(), var_leafs_start, " + ", SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, " + ", IdxCalcCodeGenerator<true, is_array_element>(array_lengths), ");");
        }
    } else {
        if (size_chain.empty()) {
            get_method = get_method
            .line(get_first_return_line_part<type_name>(), var_leafs_start, " + ", IdxCalcCodeGenerator<false, is_array_element>(array_lengths), " * ", type_size_str, ");");
        } else {
            get_method = get_method
            .line(get_first_return_line_part<type_name>(), var_leafs_start, " + ", SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, " + ", IdxCalcCodeGenerator<false, is_array_element>(array_lengths), " * ", type_size_str, ");");
        }
    }
    return std::move(get_method);
}

template <bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename Last>
INLINE codegen::Method<Last> _gen_var_value_leaf_default (
    codegen::Method<Last>&& get_method,
    OffsetsAccessor offsets_accessor
) {
    uint64_t var_leafs_start = offsets_accessor.var_leafs_start;
    const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();

    if (size_chain.empty()) {
        get_method = get_method
        .line(get_first_return_line_part<type_name>(), var_leafs_start, ");");
    } else {
        get_method = get_method
        .line(get_first_return_line_part<type_name>(), var_leafs_start, " + ", SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, ");");
    }

    return std::move(get_method);
}

template <bool in_array, bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename ArgsT, typename CodeT>
INLINE CodeT _gen_var_value_leaf (
    CodeT&& code,
    OffsetsAccessor offsets_accessor,
    ArgsT&& additional_args,
    const ArrayLengths& array_lengths
) {
    if constexpr (is_array_element) {
        auto&& get_method = code
        .method(type_name, "get", codegen::Args("uint32_t idx"));

        get_method = _gen_var_value_leaf_in_array<true, type_name, type_size>(std::move(get_method), offsets_accessor, array_lengths);

        code = get_method
        .end();
    } else {
        auto&& get_method = code
        .method(type_name, get_name(std::forward<ArgsT>(additional_args)));
        if constexpr (in_array) {
            get_method = _gen_var_value_leaf_in_array<false, type_name, type_size>(std::move(get_method), offsets_accessor, array_lengths);
        } else {
            get_method = _gen_var_value_leaf_default<false, type_name, type_size>(std::move(get_method), offsets_accessor);
        }
        code = get_method
        .end();
    }
    return std::move(code);
}

template <bool is_fixed, bool is_array_element, bool in_array, StringLiteral type_name, lexer::SIZE type_size, typename ArgsT, typename CodeT>
INLINE CodeT _gen_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    ArgsT&& name_provideing_args,
    const ArrayLengths& array_lengths
) {
    if constexpr (is_fixed) {
        return _gen_fixed_value_leaf<in_array, is_array_element, type_name, type_size>(std::move(code), offsets_accessor, std::forward<ArgsT>(name_provideing_args), array_lengths);
    } else {
        return _gen_var_value_leaf<in_array, is_array_element, type_name, type_size>(std::move(code), offsets_accessor, std::forward<ArgsT>(name_provideing_args), array_lengths);
    }
}

template <bool is_fixed, bool in_array, StringLiteral type_name, lexer::SIZE type_size, LeafArgs ArgsT, typename CodeT>
INLINE CodeT gen_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    const ArgsT& additional_args,
    const ArrayLengths& array_lengths
) {
    return _gen_value_leaf<is_fixed, is_array_element<ArgsT>, in_array, type_name, type_size>(std::move(code), offsets_accessor, additional_args, array_lengths);
}
template <bool is_fixed, bool is_array_element, bool in_array, StringLiteral type_name, lexer::SIZE type_size, typename NameT, typename CodeT>
INLINE CodeT gen_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    NameT&& name,
    const ArrayLengths& array_lengths
) {
    return _gen_value_leaf<is_fixed, is_array_element, in_array, type_name, type_size>(std::move(code), offsets_accessor, std::forward<NameT>(name), array_lengths);
}

template <typename ArgsT, typename UniqueNameT, typename CodeT>
INLINE CodeT gen_field_access_method_no_array (
    CodeT&& code,
    const ArgsT& additional_args,
    const std::string_view& ctor_used,
    UniqueNameT&& unique_name
) {
    auto&& field_method = code
    .method(unique_name, get_name(additional_args, unique_name));
    if constexpr (is_dynamic_variant_element<ArgsT>) {
        if (additional_args.offset.empty()) {
            logger::debug("[gen_field_access_method_no_array] additional_args.offset is empty");
            field_method = field_method
            .line(ctor_used);
        } else {
            field_method = field_method
            .line("return {base", additional_args.offset, std::string_view{ctor_used.data() + 12, ctor_used.size() - 12});
        }
    } else {
        field_method = field_method
        .line(ctor_used);
    }
    code = field_method
    .end();
    return std::move(code);
}

template <typename TypeT, bool is_fixed, bool in_array, typename ArgsT, typename BaseNameT>
struct TypeVisitor : lexer::ITypeVisitor<TypeT, codegen::__UnknownStruct> {
    INLINE constexpr TypeVisitor (
        const lexer::Type* const& field,
        Buffer& buffer,
        codegen::__UnknownStruct&& code,
        const BaseNameT& base_name,
        const OffsetsAccessor& offsets_accessor,
        const std::span<SizeLeaf>& level_size_leafs,
        uint16_t& current_size_leaf_idx,
        const ArgsT& additional_args,
        const ArrayLengths& array_lengths
    ) :
    lexer::ITypeVisitor<TypeT, codegen::__UnknownStruct>(field),
    buffer(buffer),
    code(std::move(code)),
    base_name(base_name),
    offsets_accessor(offsets_accessor),
    level_size_leafs(level_size_leafs),
    current_size_leaf_idx(current_size_leaf_idx),
    additional_args(additional_args),
    array_lengths(array_lengths)
    {}

    Buffer& buffer;
    codegen::__UnknownStruct&& code;
    const BaseNameT& base_name;
    const OffsetsAccessor& offsets_accessor;
    const std::span<SizeLeaf>& level_size_leafs;
    uint16_t& current_size_leaf_idx;
    const ArgsT& additional_args;
    const ArrayLengths& array_lengths;
    const uint8_t& array_depth = array_lengths.length;
    

    template <lexer::VALUE_FIELD_TYPE field_type, StringLiteral type_name>
    INLINE codegen::__UnknownStruct on_simple () const {
        return gen_value_leaf<is_fixed, in_array, type_name, lexer::get_type_alignment<field_type>()>(std::move(code), offsets_accessor, additional_args, array_lengths);
    }

    INLINE codegen::__UnknownStruct on_bool     () const override { return on_simple<lexer::VALUE_FIELD_TYPE::BOOL     ,"bool"     >(); }
    INLINE codegen::__UnknownStruct on_uint8    () const override { return on_simple<lexer::VALUE_FIELD_TYPE::UINT8    , "uint8_t" >(); }
    INLINE codegen::__UnknownStruct on_uint16   () const override { return on_simple<lexer::VALUE_FIELD_TYPE::UINT16   , "uint16_t">(); }
    INLINE codegen::__UnknownStruct on_uint32   () const override { return on_simple<lexer::VALUE_FIELD_TYPE::UINT32   , "uint32_t">(); }
    INLINE codegen::__UnknownStruct on_uint64   () const override { return on_simple<lexer::VALUE_FIELD_TYPE::UINT64   , "uint64_t">(); }
    INLINE codegen::__UnknownStruct on_int8     () const override { return on_simple<lexer::VALUE_FIELD_TYPE::INT8     , "int8_t"  >(); }
    INLINE codegen::__UnknownStruct on_int16    () const override { return on_simple<lexer::VALUE_FIELD_TYPE::INT16    , "int16_t" >(); }
    INLINE codegen::__UnknownStruct on_int32    () const override { return on_simple<lexer::VALUE_FIELD_TYPE::INT32    , "int32_t" >(); }
    INLINE codegen::__UnknownStruct on_int64    () const override { return on_simple<lexer::VALUE_FIELD_TYPE::INT64    , "int64_t" >(); }
    INLINE codegen::__UnknownStruct on_float32  () const override { return on_simple<lexer::VALUE_FIELD_TYPE::FLOAT32  , "float"   >(); }
    INLINE codegen::__UnknownStruct on_float64  () const override { return on_simple<lexer::VALUE_FIELD_TYPE::FLOAT64  , "double"  >(); }

    INLINE codegen::__UnknownStruct on_fixed_string (const lexer::FixedStringType* const fixed_string_type) const override {
        uint32_t length = fixed_string_type->length;
        auto size_type_str =  get_size_type_str(fixed_string_type->length_size);
        // auto offset = offsets_accessor.next_offset<is_fixed>();

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto unique_name = get_unique_name<"String">(additional_args);

        auto&& string_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        string_struct = gen_value_leaf<is_fixed, is_array_element<ArgsT>, in_array, "char*", lexer::SIZE::SIZE_1>(std::move(string_struct), offsets_accessor, "c_str"_sl, array_lengths);

        string_struct = string_struct
            .method(codegen::Attributes("constexpr"), size_type_str, "size")
                .line("return ", length, ";")
            .end()
            .method(codegen::Attributes("constexpr"), size_type_str, "length")
                .line("return size() - 1;")
            .end()
            ._private()
            .field("size_t", "base");

        for (uint8_t i = 0; i < array_depth; i++) {
            string_struct = string_struct
            .field("uint32_t", codegen::StringParts("idx_", i));
        }
        if constexpr (is_dynamic_variant_element<ArgsT>) {
            if (level_size_leafs.size() > 0) {
                logger::error("Unexpected state");
            }
        }
        
        code = string_struct
        .end();

        if constexpr (is_array_element<ArgsT>) {
            code = code
            .method(unique_name, "get", codegen::Args("uint32_t idx"))
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            code = gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
        }
        return std::move(code);
    }

    INLINE codegen::__UnknownStruct on_string (const lexer::StringType* const string_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Variable length strings in arrays are not supported");
        } else {
            lexer::SIZE size_size = string_type->size_size;
            lexer::SIZE stored_size_size = string_type->stored_size_size;
            auto size_type_str = get_size_type_str(size_size);
            const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();

            ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

            uint16_t size_leaf_idx = current_size_leaf_idx++;
            logger::debug("STRING size_leaf_idx: ", size_leaf_idx);

            auto unique_name = get_unique_name(additional_args);

            auto&& string_data_method = code
            ._struct(unique_name)
                .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end()
                .method("const char*", "c_str");

            if (size_chain.empty()) {
                string_data_method = string_data_method
                .line("return reinterpret_cast<const char*>(base + ", offsets_accessor.var_leafs_start, ");");
            } else {
                string_data_method = string_data_method
                .line("return reinterpret_cast<const char*>(base + ", offsets_accessor.var_leafs_start, SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, ");");
            }

            auto&& string_size_method = 
            string_data_method
            .end()
            .method(size_type_str, "size");
            
            if constexpr (is_dynamic_variant_element<ArgsT>) {
                uint64_t offset = offsets_accessor.next_fixed_offset();
                string_size_method = string_size_method
                .line("return ", string_type->min_length, " + *reinterpret_cast<", get_size_type_str(stored_size_size), "*>(base + ", offset, ");");
            } else {
                level_size_leafs[size_leaf_idx] = {
                    string_type->min_length,
                    offsets_accessor.next_map_idx(),
                    size_size,
                    stored_size_size
                };
                string_size_method = string_size_method
                .line("return size", size_leaf_idx, "(base);");
            }

            code =
                    string_size_method
                .end()
                .method(size_type_str, "length")
                    .line("return size() - 1;")
                .end()
                ._private()
                .field("size_t", "base")
            .end();
            code = gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);

            return std::move(code);
        }
    }

    INLINE TypeVisitor::ResultT on_fixed_array (const lexer::ArrayType* const fixed_array_type) const override {
        uint32_t length = fixed_array_type->length;
        auto size_type_str = get_size_type_str(fixed_array_type->size_size);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        uint16_t depth;
        if constexpr (std::is_same_v<ArgsT, GenFixedArrayLeafArgs>) {
            // only fixed arrays can be nested
            depth = additional_args.depth + 1;
        } else {
            depth = 0;
        }

        auto unique_name = get_unique_name(additional_args, [&depth]() { return codegen::StringParts{"Array_"_sl, depth}; });

        auto&& array_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        uint32_t new_array_lengths[array_depth];
        if (array_depth > 0) {
            const uint8_t last_i = array_depth - 1;
            for (uint8_t i = 0; i < last_i; i++) {
                new_array_lengths[i] = array_lengths.data[i];
            }
            new_array_lengths[last_i] = length;
        }

        typename TypeVisitor::ResultT result = TypeVisitor<
            TypeT,
            is_fixed,
            true,
            GenFixedArrayLeafArgs,
            std::conditional_t<is_dynamic_variant_element<ArgsT>, decltype(unique_name), decltype(base_name)>
        >{
            fixed_array_type->inner_type(),
            buffer,
            codegen::__UnknownStruct{std::move(array_struct)},
            conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name),
            offsets_accessor,
            level_size_leafs,
            current_size_leaf_idx,
            GenFixedArrayLeafArgs{depth},
            ArrayLengths{new_array_lengths, static_cast<uint8_t>(array_depth + 1)}
        }.visit();

        array_struct = ((decltype(array_struct))result.value)
            .method(codegen::Attributes("constexpr"), size_type_str, "length")
                .line("return ", length, ";")
            .end()
            ._private()
            .field("size_t", "base");

        for (uint8_t i = 0; i < array_depth; i++) {
            array_struct = array_struct
            .field("uint32_t", codegen::StringParts("idx_", i));
        }
        if constexpr (is_dynamic_variant_element<ArgsT>) {
            if (level_size_leafs.size() > 0) {
                INTERNAL_ERROR("Unexpected state");
            }
        }

        code = array_struct
        .end();
        
        if constexpr (is_array_element<ArgsT>) {
            code = code
            .method(unique_name, "get", codegen::Args("uint32_t idx"))
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            code = gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
        }
        return {
            std::move(result.next_type),
            std::move(code)
        };
    }


    INLINE TypeVisitor::ResultT on_array (const lexer::ArrayType* const array_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
            lexer::SIZE size_size = array_type->size_size;
            lexer::SIZE stored_size_size = array_type->stored_size_size;
            auto size_type_str = get_size_type_str(size_size);

            ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(0);

            auto unique_name = get_unique_name(additional_args);

            auto&& array_struct = code
            ._struct(unique_name)
                .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

            typename TypeVisitor::ResultT result = TypeVisitor<
                TypeT,
                false,
                true,
                GenArrayLeafArgs,
                std::conditional_t<is_dynamic_variant_element<ArgsT>, decltype(unique_name), decltype(base_name)>
            >{
                array_type->inner_type(),
                buffer,
                codegen::__UnknownStruct{std::move(array_struct)},
                conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name),
                offsets_accessor,
                level_size_leafs,
                current_size_leaf_idx,
                GenArrayLeafArgs{},
                ArrayLengths{nullptr, 1}
            }.visit();

            uint16_t size_leaf_idx = current_size_leaf_idx++;
            logger::debug("ARRAY size_leaf_idx:", size_leaf_idx);
            level_size_leafs[size_leaf_idx] = {
                array_type->length,
                offsets_accessor.next_map_idx(),
                size_size,
                stored_size_size
            };

            array_struct = ((decltype(array_struct))result.value)
                .method(size_type_str, "length")
                    .line("return size", size_leaf_idx, "(base);")
                .end()
                ._private()
                .field("size_t", "base");
            if constexpr (is_dynamic_variant_element<ArgsT>) {
                array_struct = add_size_leafs(level_size_leafs, offsets_accessor.offsets.fixed_offsets, std::move(array_struct));
            }
            code = array_struct
            .end();
            code = gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);

            return {
                std::move(result.next_type),
                std::move(code)
            };
        }
    }

    INLINE TypeVisitor::ResultT on_fixed_variant (const lexer::FixedVariantType* const fixed_variant_type) const override {
        uint16_t variant_count = fixed_variant_type->variant_count;

        auto unique_name = get_unique_name<"Variant">(additional_args);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);
        
        auto&& variant_struct = code
        ._struct(unique_name);

        variant_struct = variant_struct
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();
        // auto id_leaf_offset = offsets_accessor.next_offset<is_fixed>();
        if (variant_count <= UINT8_MAX) {
            variant_struct = gen_value_leaf<is_fixed, is_array_element<ArgsT>, in_array, "uint8_t", lexer::SIZE::SIZE_1>(std::move(variant_struct), offsets_accessor, "id"_sl, array_lengths);
        } else {
            variant_struct = gen_value_leaf<is_fixed, is_array_element<ArgsT>, in_array, "uint16_t", lexer::SIZE::SIZE_2>(std::move(variant_struct), offsets_accessor, "id"_sl, array_lengths);
        }              

        auto type = fixed_variant_type->first_variant();
        uint64_t max_offset = 0;

        uint16_t variant_depth;
        if constexpr (is_variant_element<ArgsT>) {
            variant_depth = additional_args.variant_depth + 1;
        } else {
            variant_depth = 0;
        }

        uint64_t max_size = 12345; // TODO. figure out how to store this when genrating offsets.
        
        for (uint16_t i = 0; i < variant_count; i++) {
            auto& type_meta = fixed_variant_type->type_metas()[i];
            auto& fixed_leaf_counts = type_meta.fixed_leaf_counts.counts;
            constexpr auto var_leaf_counts = lexer::LeafCounts::zero().counts;
            auto& variant_field_counts = type_meta.variant_field_counts.counts;
            uint16_t total_fixed_leafs = fixed_leaf_counts.total();
            uint16_t total_var_leafs = var_leaf_counts.total();
            uint16_t level_variant_fields = variant_field_counts.total();
            constexpr uint16_t total_size_leafs = 0;

            // GenFixedVariantLeafArgs<in_array> gen_variant_leaf_args;
            // if constexpr (in_array) {
            //     gen_variant_leaf_args = GenFixedVariantLeafArgs<true>{{i, variant_depth}, max_size, idx_calc_str};
            // } else {
            //     gen_variant_leaf_args = GenFixedVariantLeafArgs<false>{{i, variant_depth}};
            // }
            
            lexer::ITypeVisitorResult<lexer::Type, codegen::__UnknownStruct> result = TypeVisitor<
                lexer::Type,
                is_fixed,
                in_array,
                GenFixedVariantLeafArgs,
                std::conditional_t<is_dynamic_variant_element<ArgsT>, decltype(unique_name), decltype(base_name)>
            >{
                type,
                buffer,
                codegen::__UnknownStruct{std::move(variant_struct)},
                conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name),
                offsets_accessor,
                std::span<SizeLeaf>{},
                current_size_leaf_idx,
                GenFixedVariantLeafArgs{i, variant_depth},
                array_lengths
            }.visit();

            type = result.next_type;
            variant_struct = (decltype(variant_struct))result.value;
        }
        
        variant_struct = variant_struct
        ._private()
            .field("size_t", "base");

        for (uint8_t i = 0; i < array_depth; i++) {
            variant_struct = variant_struct
            .field("uint32_t", codegen::StringParts("idx_", i));
        }
        if constexpr (is_dynamic_variant_element<ArgsT>) {
            variant_struct = add_size_leafs(level_size_leafs, offsets_accessor.offsets.fixed_offsets, std::move(variant_struct));
        }

        code = variant_struct
        .end();

        

        if constexpr (is_array_element<ArgsT>) {
            code = code
            .method("Variant", "get", codegen::Args("uint32_t idx"))
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            // code = code
            // .method(unique_name, get_name(additional_args, unique_name))
            //     .line(array_ctor_strs.ctor_used)
            // .end();
            code = gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
        }

        return {
            reinterpret_cast<TypeVisitor::ConstTypeT*>(type),
            std::move(code)
        };
    }

    INLINE TypeVisitor::ResultT on_packed_variant (const lexer::PackedVariantType* const packed_variant_type) const override {
        INTERNAL_ERROR("Packed variant not supported");
    }

    INLINE TypeVisitor::ResultT on_dynamic_variant (const lexer::DynamicVariantType* const dynamic_variant_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
            uint16_t variant_count = dynamic_variant_type->variant_count;

            auto unique_name = get_unique_name<"DynamicVariant">(additional_args);

            ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);
            
            auto&& variant_struct = code
            ._struct(unique_name)
                .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();
            //auto id_offset = offsets_accessor.next_offset<is_fixed>();
            if (variant_count <= UINT8_MAX) {
                variant_struct = gen_value_leaf<is_fixed, is_array_element<ArgsT>, in_array, "uint8_t", lexer::SIZE::SIZE_1>(std::move(variant_struct), offsets_accessor, "id"_sl, array_lengths);
            } else {
                variant_struct = gen_value_leaf<is_fixed, is_array_element<ArgsT>, in_array, "uint16_t", lexer::SIZE::SIZE_2>(std::move(variant_struct), offsets_accessor, "id"_sl, array_lengths);
            }

            uint16_t size_leaf_idx = current_size_leaf_idx++;
            logger::debug("ARRAY size_leaf_idx: ", size_leaf_idx);
            level_size_leafs[size_leaf_idx] = {
                dynamic_variant_type->min_byte_size,
                offsets_accessor.next_map_idx(),
                dynamic_variant_type->size_size,
                dynamic_variant_type->stored_size_size
            };

            const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();
            
            size_t offset_str_size;
            const uint64_t* size_chain_data;
            if (size_chain.empty()) {
                offset_str_size = 0;
            } else {
                size_chain_data = offsets_accessor.var_offset_buffer.get_aligned(size_chain.start_idx);
                offset_str_size = SizeChainCodeGenerator(size_chain_data, size_chain.length).get_size();
            }
            std::string_view offset;
            char offset_str_buf[offset_str_size];
            if (size_chain.empty()) {
                offset = std::string_view(nullptr, 0);
            } else {
                char* end = SizeChainCodeGenerator(size_chain_data, size_chain.length).write(offset_str_buf);
                // if (offset_str_size != end - offset_str_buf) {
                //     logger::warn("offset_str_size != dst - offset_str_buf, ", offset_str_size, " != ", reinterpret_cast<size_t>(end), " - ", reinterpret_cast<size_t>(offset_str_buf));
                // }
                offset = std::string_view(offset_str_buf, offset_str_size);
            }

            auto type = dynamic_variant_type->first_variant();

            uint16_t variant_depth;
            if constexpr (is_variant_element<ArgsT>) {
                variant_depth = additional_args.variant_depth + 1;
            } else {
                variant_depth = 0;
            }

            uint64_t max_size = 12345; // TODO. figure out how to store this when genrating offsets.
            
            for (uint16_t i = 0; i < variant_count; i++) {
                auto type_meta = dynamic_variant_type->type_metas()[i];
                auto fixed_leaf_counts = type_meta.fixed_leaf_counts.counts;
                auto var_leaf_counts = type_meta.var_leaf_counts.counts;
                auto variant_field_counts = type_meta.variant_field_counts.counts;
                uint16_t total_fixed_leafs = fixed_leaf_counts.total();
                uint16_t total_var_leafs = var_leaf_counts.total();
                uint16_t level_variant_fields = variant_field_counts.total();
                uint16_t level_size_leafs_count = type_meta.level_size_leafs;

                SizeLeaf level_size_leafs[level_size_leafs_count];

                // GenFixedVariantLeafArgs<in_array> gen_variant_leaf_args;
                // if constexpr (in_array) {
                //     gen_variant_leaf_args = GenFixedVariantLeafArgs<true>{{i, variant_depth}, max_size, idx_calc_str};
                // } else {
                //     gen_variant_leaf_args = GenFixedVariantLeafArgs<false>{{i, variant_depth}};
                // }
                uint16_t current_size_leaf_idx = 0;
                lexer::ITypeVisitorResult<TypeT, codegen::__UnknownStruct> result = TypeVisitor<
                    TypeT,
                    true,
                    in_array,
                    GenDynamicVariantLeafArgs,
                    std::conditional_t<is_dynamic_variant_element<ArgsT>, decltype(unique_name), decltype(base_name)>
                >{
                    type,
                    buffer,
                    codegen::__UnknownStruct{std::move(variant_struct)},
                    conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name),
                    offsets_accessor,
                    std::span<SizeLeaf>{level_size_leafs, level_size_leafs + level_size_leafs_count},
                    current_size_leaf_idx,
                    GenDynamicVariantLeafArgs{
                        {i, variant_depth},
                        offset
                    },
                    array_lengths
                }.visit();
                type = (lexer::Type*)result.next_type;
                variant_struct = (decltype(variant_struct))result.value;
            }

            variant_struct = variant_struct
            ._private()
                .field("size_t", "base");

            for (uint8_t i = 0; i < array_depth; i++) {
                variant_struct = variant_struct
                .field("uint32_t", codegen::StringParts("idx_", i));
            }
            if constexpr (is_dynamic_variant_element<ArgsT>) {
                logger::debug("Adding size leafs, variant_depth: ", additional_args.variant_depth);
                variant_struct = add_size_leafs(level_size_leafs, offsets_accessor.offsets.fixed_offsets, std::move(variant_struct));
            }
            code = variant_struct
            .end();

            if constexpr (is_array_element<ArgsT>) {
                static_assert(false, "Dynamic array cant be array element");
                // code = code
                // .method("Variant", "get", codegen::Args("uint32_t idx"))
                //     .line("/* unreachable */ return {base, base + ", offset, " + ", IdxCalcCodeGenerator<false, true>(array_lengths), " * ", max_size, std::string_view{array_ctor_strs.el_ctor_used.data() + 12, array_ctor_strs.el_ctor_used.length() - 12})
                // .end();
            } else {
                // auto&& variant_get_method = code
                // .method(unique_name, get_name(additional_args, unique_name));
                if constexpr (!in_array) {
                    // code = code
                    // .method(unique_name, get_name(additional_args, unique_name))
                    //     .line(array_ctor_strs.ctor_used)
                    // .end();
                    code = gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
                    // variant_get_method = variant_get_method
                    // .line("return {base, base + ", offset, std::string_view{array_ctor_strs.ctor_used.data() + 12, array_ctor_strs.ctor_used.length() - 12});
                } else {
                    static_assert(false, "Dynamic array cant bein array");
                    // variant_get_method = variant_get_method
                    // .line("/* unreachable */ return {base, base + ", offset, std::string_view{array_ctor_strs.ctor_used.data() + 12, array_ctor_strs.ctor_used.length() - 12}, " + ", IdxCalcCodeGenerator<false>(array_lengths), " * ", max_size);
                }
                // code = variant_get_method
                // .end();
            }

            
            return {
                reinterpret_cast<typename TypeVisitor::ConstTypeT*>(type),
                std::move(code)
            };
        }
    }

    INLINE codegen::__UnknownStruct on_identifier (const lexer::IdentifiedType* const identified_type) const override {
        auto identifier = buffer.get(identified_type->identifier_idx);
        if (identifier->keyword != lexer::KEYWORDS::STRUCT) {
            INTERNAL_ERROR("not implemented");
        }
        auto struct_type = identifier->data()->as_struct();

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto unique_name = get_unique_name(additional_args, [&struct_type]() { return std::string_view{struct_type->name.offset, struct_type->name.length}; });
        
        auto&& struct_code = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        auto field = struct_type->first_field();
        for (uint16_t i = 0; i < struct_type->field_count; i++) {
            auto field_data = field->data();
            auto name = std::string_view{field_data->name.offset, field_data->name.length};
            uint16_t struct_depth;
            if constexpr (std::is_same_v<ArgsT, GenStructLeafArgs>) {
                struct_depth = additional_args.depth + 1;
            } else {
                struct_depth = 0;
            }

            lexer::ITypeVisitorResult<lexer::StructField, codegen::__UnknownStruct> result = TypeVisitor<
                lexer::StructField,
                is_fixed,
                in_array,
                GenStructLeafArgs,
                std::conditional_t<is_dynamic_variant_element<ArgsT>, decltype(unique_name), decltype(base_name)>
            >{
                field_data->type(),
                buffer,
                codegen::__UnknownStruct{std::move(struct_code)},
                conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name),
                offsets_accessor,
                level_size_leafs,
                current_size_leaf_idx,
                GenStructLeafArgs{name, struct_depth},
                array_lengths
            }.visit();
        
            field = result.next_type;
            struct_code = (decltype(struct_code))result.value;
        }

        struct_code = struct_code
            ._private()
            .field("size_t", "base");
        
        for (uint8_t i = 0; i < array_depth; i++) {
            struct_code = struct_code
            .field("uint32_t", codegen::StringParts("idx_", i));
        }
        if constexpr (is_dynamic_variant_element<ArgsT>) {
            struct_code = add_size_leafs(level_size_leafs, offsets_accessor.offsets.fixed_offsets, std::move(struct_code));
        }

        code = struct_code
        .end();

        if constexpr (is_array_element<ArgsT>) {
            code = code
            .method(unique_name, "get", codegen::Args("uint32_t idx"))
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            code = gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
        }

        return std::move(code);
    }
};


// TODO: 
// current_size_leaf_idx only is needed for var size leafs


static INLINE void handled_write (int fd, const char* buf, size_t size) {
    int result = write(fd, buf, size);
    if (result < 0) {
        INTERNAL_ERROR("[handled_write] write failed, ERRNO: ", errno);
    }
}

template <size_t size>
requires (is_power_of_two(size))
struct Writer {
    char buffer[size];
    size_t position = 0;
    const int fd;
    Writer (int fd) : fd(fd) {}

    void write (std::string_view str) {
        const char* start = str.data();
        write(start, start + str.size());
    }

    void write (const char* start, const char* end) {
        size_t length = end - start;

        size_t free_space = size - position;
        if (free_space >= length) {
            memcpy(buffer + position, start, length);
            position += length;
            if (position == size) {
                handled_write(fd, buffer, size);
                position = 0;
            }
        } else {
            memcpy(buffer + position, start, free_space);
            handled_write(fd, buffer, size);
            start += free_space;
            while (start <= end - size) {
                memcpy(buffer, start, size);
                handled_write(fd, buffer, size);
                start += size;
            }
            if (start < end) {
                size_t remaining = end - start;
                memcpy(buffer, start, remaining);
                position = remaining;
            } else {
                position = 0;
            }
        }
        
    }

    void done () {
        if (position == 0) {
            return;
        }
        logger::debug("Wrinting to fd: ", fd);
        handled_write(fd, buffer, position);
        position = 0;
    }

    ~Writer () {
        // Auto call done
        done();
    }
};

void print_leafs (const char* name, lexer::LeafCounts leafs) {
    logger::debug(name, ": {8:", leafs.counts.size8, ", 16:", leafs.counts.size16, ", 32:", leafs.counts.size32, ", 64:", leafs.counts.size64, ", total:", leafs.total(), "}");
}

void generate (
    const lexer::StructDefinition* target_struct,
    Buffer& buffer,
    const int output_fd
) {
    uint8_t _buffer[5000];
    auto code = codegen::create_code(_buffer)
    .line("#include \"lib/lib.hpp\"")
    .line("");
    auto fixed_leaf_counts = target_struct->fixed_leaf_counts.counts;
    auto var_leaf_counts = target_struct->var_leafs_count.counts;
    auto variant_field_counts = target_struct->variant_field_counts.counts;
    uint16_t total_fixed_leafs = fixed_leaf_counts.total();
    uint16_t total_var_leafs = var_leaf_counts.total();
    uint16_t level_variant_fields = variant_field_counts.total();
    uint16_t total_variant_fixed_leafs = target_struct->total_variant_fixed_leafs;
    uint16_t total_variant_var_leafs = target_struct->total_variant_var_leafs;
    uint16_t total_leafs = total_fixed_leafs + total_var_leafs + total_variant_fixed_leafs  + total_variant_var_leafs;
    uint16_t variant_base_idx = total_fixed_leafs + total_var_leafs;
    uint16_t level_size_leafs_count = target_struct->level_size_leafs;
    uint16_t total_variant_count = target_struct->total_variant_count;
    print_leafs("fixed_leaf_counts", fixed_leaf_counts);
    print_leafs("var_leaf_counts", var_leaf_counts);
    print_leafs("variant_field_counts", variant_field_counts);
    logger::debug("level_variant_fields: ", target_struct->level_variant_fields, " vs ", level_variant_fields);
    logger::debug("total_variant_fixed_leafs: ", total_variant_fixed_leafs);
    logger::debug("total_variant_var_leafs: ", total_variant_var_leafs);
    logger::debug("total_leafs: ", total_leafs);
    logger::debug("level_size_leafs: ", level_size_leafs_count);
    logger::debug("total_variant_count: ", total_variant_count);

    std::string_view struct_name = std::string_view{target_struct->name.offset, target_struct->name.length};
    logger::debug("fixed_offsets length: ", total_fixed_leafs + total_variant_fixed_leafs);
    uint64_t fixed_offsets[total_fixed_leafs + total_variant_fixed_leafs];
    logger::debug("var_offsets length: ", total_var_leafs + total_variant_var_leafs);
    Buffer::View<uint64_t> var_offsets[total_var_leafs + total_variant_var_leafs];
    logger::debug("idx_map length: ", total_leafs);
    uint16_t idx_map[total_leafs];
    logger::debug("variant_offsets length: ", total_variant_count);
    uint64_t variant_offsets[total_variant_count];

    const Offsets offsets = {fixed_offsets, var_offsets, variant_offsets, idx_map};

    Buffer var_offset_buffer = {4096};

    for (size_t i = 0; i < 10'000'000; ++i) {
        volatile auto generate_offsets_info = generate_offsets::generate(
            target_struct,
            buffer,
            offsets,
            var_offset_buffer,
            fixed_leaf_counts,
            var_leaf_counts,
            variant_field_counts,
            total_fixed_leafs,
            total_var_leafs,
            level_variant_fields,
            level_size_leafs_count
        );
        var_offset_buffer.clear();
    }
    auto generate_offsets_info = generate_offsets::generate(
        target_struct,
        buffer,
        offsets,
        var_offset_buffer,
        fixed_leaf_counts,
        var_leaf_counts,
        variant_field_counts,
        total_fixed_leafs,
        total_var_leafs,
        level_variant_fields,
        level_size_leafs_count
    );
    
    uint16_t current_map_idx = 0;
    uint16_t current_variant_field_idx = 0;
    OffsetsAccessor offsets_accessor = {
        offsets,
        var_offset_buffer,
        generate_offsets_info.var_leafs_start,
        current_map_idx,
        current_variant_field_idx
    };

    SizeLeaf _level_size_leafs[level_size_leafs_count];
    uint16_t current_size_leaf_idx = 0;

    std::span<SizeLeaf> level_size_leafs = {_level_size_leafs, _level_size_leafs + level_size_leafs_count};
    
    
    auto&& struct_code = code
    ._struct(struct_name)
        .ctor("size_t base", "base(base)").end();

    auto field = target_struct->first_field();
    for (uint16_t i = 0; i < target_struct->field_count; i++) {
        auto field_data = field->data();
        auto name = std::string_view{field_data->name.offset, field_data->name.length};
        // auto result = gen_leaf<true, false>(
        //     field_data->type(),
        //     buffer,
        //     codegen::__UnknownStruct{std::move(struct_code)},
        //     struct_name,
        //     offsets_accessor,
        //     level_size_leafs,
        //     current_size_leaf_idx,
        //     GenStructLeafArgs{name, 0},
        //     ArrayLengths{nullptr, 0}
        // );
        //field = result.next;
        //struct_code = (decltype(struct_code))result.code;
        auto result = TypeVisitor<
            lexer::StructField,
            true,
            false,
            GenStructLeafArgs,
            decltype(struct_name)
        >{
            field_data->type(),
            buffer,
            codegen::__UnknownStruct{std::move(struct_code)},
            struct_name,
            offsets_accessor,
            level_size_leafs,
            current_size_leaf_idx,
            GenStructLeafArgs{name, 0},
            ArrayLengths{nullptr, 0}
        }.visit();

        field = result.next_type;
        struct_code = (decltype(struct_code))result.value;
        
    }

    var_offset_buffer.dispose();

    struct_code = struct_code
        ._private()
        .field("size_t", "base");

    struct_code = add_size_leafs(level_size_leafs, offsets.fixed_offsets, std::move(struct_code));

    auto code_done = struct_code
    .end()
    .end();


    Buffer code_buffer = code_done.buffer();
    auto writer = Writer<4096>(output_fd);
    writer.write(code_buffer.get<char>({0}), code_buffer.get<char>({code_buffer.current_position()}));
    writer.done();

    code_done.dispose();
}


}