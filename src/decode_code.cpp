#pragma once
#include <alloca.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <gsl/pointers>
#include <gsl/util>
#include <span>
#include <string>
#include <array>
#include <string_view>
#include <type_traits>
#include <utility>
//#include "generate_offsets.cpp"
#include "./codegen.hpp"
#include "./estd/concepts.hpp"
#include "./parser/lexer_types.hpp"
#include "./container/memory.hpp"
#include "./util/string_literal.hpp"
#include "./util/logger.hpp"
#include "./helper/internal_error.hpp"
#include "./estd/meta.hpp"
#include "./fast_math/sum_of_digits.hpp"
#include "./fast_math/log.hpp"
#include "./code_gen_stuff.hpp"
#include "./generate_offsets.cpp"
#include "estd/empty.hpp"
#include "variant_optimizer/data.hpp"

namespace decode_code {

struct SizeLeaf {
    uint64_t min_size;
    uint16_t idx;
    lexer::SIZE size_size;
    lexer::SIZE stored_size_size;
};

struct OffsetsAccessor {
    OffsetsAccessor (
        const FixedOffset* fixed_offsets,
        const Buffer::View<uint64_t>* var_offsets,
        const uint16_t* idx_map,
        ReadOnlyBuffer var_offset_buffer,
        uint64_t var_leafs_start,
        gsl::not_null<uint16_t*> current_map_idx,
        gsl::not_null<uint16_t*> current_variant_field_idx
    ) :
    fixed_offsets(fixed_offsets),
    var_offsets(var_offsets),
    idx_map(idx_map),
    var_offset_buffer(var_offset_buffer),
    var_leafs_start(var_leafs_start),
    current_map_idx(current_map_idx),
    current_variant_field_idx(current_variant_field_idx)
    {}
    const FixedOffset* fixed_offsets;
    const Buffer::View<uint64_t>* var_offsets;
    const uint16_t* idx_map;
    ReadOnlyBuffer var_offset_buffer;
    uint64_t var_leafs_start;
    gsl::not_null<uint16_t*> current_map_idx;
    gsl::not_null<uint16_t*> current_variant_field_idx;

    [[nodiscard]] uint16_t next_map_idx () const {
        uint16_t map_idx = (*current_map_idx)++;
        logger::debug("next_map_idx: ", map_idx);
        return idx_map[map_idx];
    }

    [[nodiscard]] uint16_t next_variant_field_idx () const {
        uint16_t variant_field_idx = (*current_variant_field_idx)++;
        logger::debug("next_variant_field_idx: ", variant_field_idx);
        return variant_field_idx;
    }

    [[nodiscard]] uint64_t next_fixed_offset () const {
        return fixed_offsets[next_map_idx()].get_offset();
    }

    [[nodiscard]] FixedOffset next_fixed_leaf () const {
        return fixed_offsets[next_map_idx()];
    }

    [[nodiscard]] Buffer::View<uint64_t> next_var_offset () const {
        auto idx = next_map_idx();
        auto offset = var_offsets[idx];
        logger::debug("next_var_offset at: ", idx, ", start_idx: ", offset.start_idx.value, ", length: ", offset.length);
        return offset;
    }
};

[[nodiscard]] constexpr std::string get_size_type_str (lexer::SIZE size) {
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
[[nodiscard]] constexpr ArrayCtorStrs make_array_ctor_strs (uint8_t array_depth) {
    if (array_depth > array_ctor_strs_count) {
        INTERNAL_ERROR("[make_array_ctor_strs] array depth too large");
    }
    auto [ctor_args, ctor_inits, ctor_used, el_ctor_used] = g_array_ctor_strs.strs_array[array_depth];
    return ArrayCtorStrs{
        {g_array_ctor_strs.data.data() + ctor_args.start, ctor_args.length},
        {g_array_ctor_strs.data.data() + ctor_inits.start, ctor_inits.length},
        {g_array_ctor_strs.data.data() + ctor_used.start, ctor_used.length},
        {g_array_ctor_strs.data.data() + el_ctor_used.start, el_ctor_used.length}
    };
}

struct ArrayLengths {
    const uint32_t* data;
    uint8_t length;
};

struct SizeChainCodeGenerator : stringify::Generator<size_t> {
    SizeChainCodeGenerator(
        const ReadOnlyBuffer& var_offset_buffer,
        const Buffer::View<uint64_t>& size_chain
    ) :
    size_chain_data(var_offset_buffer.get_aligned(size_chain.start_idx)),
    size_chain_length(size_chain.length)
    {}

    SizeChainCodeGenerator(
        const uint64_t* const size_chain_data,
        const Buffer::View<uint64_t>::length_t size_chain_length
    ) :
    size_chain_data(size_chain_data),
    size_chain_length(size_chain_length)
    {}

    const uint64_t* size_chain_data;
    Buffer::View<uint64_t>::length_t size_chain_length;
    
    char* write(char* dst) const override {
        for (Buffer::index_t i = 0; i < size_chain_length; i++) {
            dst = stringify::write_string(dst, " + size"_sl);
            dst = stringify::write_string(dst, i);
            dst = stringify::write_string(dst, "(base)"_sl);
            const uint64_t size = size_chain_data[i];
            if (size != 1) {
                dst = stringify::write_string(dst, " * "_sl);
                dst = stringify::write_string(dst, size);
            }
        }
        return dst;
    }

    [[nodiscard]] size_t get_size() const override {
        size_t offset_str_size = ( size_chain_length * (" + size"_sl.size() + "(base)"_sl.size()) ) + fast_math::sum_of_digits_unsafe(size_chain_length);
        for (Buffer::index_t i = 0; i < size_chain_length; i++) {
            const uint64_t size = size_chain_data[i];
            if (size != 1) {
                offset_str_size += " * "_sl.size() + fast_math::log_unsafe<10>(size) + 1;
            }
        }
        return offset_str_size;
    }
};

template <bool no_multiply, bool last_is_direct = false>
struct IdxCalcCodeGenerator : stringify::OverAllocatedGenerator<Buffer::index_t> {
    explicit IdxCalcCodeGenerator (const ArrayLengths& array_lengths) :
    array_lengths(array_lengths),
    estimated_size(estimate_size(array_lengths))
    {}

    ArrayLengths array_lengths;
    Buffer::index_t estimated_size;

    [[nodiscard]] static Buffer::index_t estimate_size (const ArrayLengths& array_lengths) {
        if (array_lengths.length == 0) {
            INTERNAL_ERROR("[IdxCalcCodeGenerator::estimate_size] array_lengths.length must be > 0")
        } else if (array_lengths.length == 1) {
            if constexpr (last_is_direct) {
                return " + idx"_sl.size();
            } else {
                return " + idx_0"_sl.size();
            }
        } else {
            Buffer::index_t size;

            if constexpr (last_is_direct) {
                size = (array_lengths.length - 1) * (" + idx_"_sl.size() + " * "_sl.size() + 19) + "idx"_sl.size() + fast_math::sum_of_digits_unsafe(gsl::narrow_cast<uint8_t>(array_lengths.length - 1));
            } else {
                size = array_lengths.length * " + idx_"_sl.size() + (array_lengths.length - 1) * (" * "_sl.size() + 19) + fast_math::sum_of_digits_unsafe(array_lengths.length);
            }
    
            if constexpr (!no_multiply) {
                size += "("_sl.size() + ")"_sl.size();
            }

            return size;
        }
    }

    [[nodiscard]] uint64_t get_idx_size_multiplier (uint8_t i) const {
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
                dst = stringify::write_string(dst, " + idx");
                goto done;
            } else {
                dst = stringify::write_string(dst, " + idx_0");
                goto done;
            }
        } else {  
            if constexpr (!no_multiply) {
                dst = stringify::write_string(dst, "(");
            }
    
            for (uint32_t i = 0; i < array_lengths.length - 1; i++) {
                dst = stringify::write_string(dst, " + idx_");
                dst = stringify::write_string(dst, i);
                dst = stringify::write_string(dst, " * ");
                dst = stringify::write_string(dst, get_idx_size_multiplier(i));
            }

            if constexpr (last_is_direct) {
                dst = stringify::write_string(dst, " + idx");
            } else {
                dst = stringify::write_string(dst, " + idx_");
                dst = stringify::write_string(dst, array_lengths.length - 1);
            }
    
            if constexpr (!no_multiply) {
                dst = stringify::write_string(dst, ")");
            }
        }
        done:;
        const Buffer::index_t allocated_size = gsl::narrow_cast<Buffer::index_t>(dst - start);
        const Buffer::index_t over_allocation = estimated_size - allocated_size;
        return {dst, over_allocation};
    }

    [[nodiscard]] Buffer::index_t get_size () const override {
        return estimated_size;
    }
};

struct GenStructFieldResult {
    lexer::StructField* next;
    codegen::UnknownStructBase code;
};

struct GenFixedArrayLeafArgs {
    uint16_t depth;
};

struct GenArrayLeafArgs {
};

struct GenVariantLeafArgsBase {
    constexpr GenVariantLeafArgsBase (uint16_t variant_id, uint16_t variant_depth)
    : variant_id(variant_id), variant_depth(variant_depth)
    {}
    uint16_t variant_id;
    uint16_t variant_depth;
};

struct GenFixedVariantLeafArgs : GenVariantLeafArgsBase {
    constexpr explicit GenFixedVariantLeafArgs (GenVariantLeafArgsBase base)
    : GenVariantLeafArgsBase(base)
    {}
};

struct GenDynamicVariantLeafArgs : GenVariantLeafArgsBase {
    constexpr GenDynamicVariantLeafArgs (std::string_view offset, GenVariantLeafArgsBase base)
    : GenVariantLeafArgsBase(base), offset(offset)
    {}
    std::string_view offset;
};

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
constexpr bool is_fixed_variant_element = std::is_same_v<ArgsT, GenFixedVariantLeafArgs>;


template <LeafArgs ArgsT>
constexpr bool is_dynamic_variant_element = std::is_same_v<ArgsT, GenDynamicVariantLeafArgs>;

template <LeafArgs ArgsT>
constexpr bool is_variant_element = is_fixed_variant_element<ArgsT> || is_dynamic_variant_element<ArgsT>;

template <LeafArgs ArgsT>
constexpr bool is_struct_element = std::is_same_v<ArgsT, GenStructLeafArgs>;

[[nodiscard]] constexpr auto get_unique_name (const GenStructLeafArgs& additional_args) {
    return codegen::StringParts{additional_args.name, "_"_sl, additional_args.depth};
}
template <StringLiteral element_name>
[[nodiscard]] constexpr auto get_unique_name (const GenStructLeafArgs& additional_args) {
    return codegen::StringParts{additional_args.name, "_"_sl, additional_args.depth};
}
template <typename F>
[[nodiscard]] constexpr auto get_unique_name (const GenStructLeafArgs& additional_args, F&& /*unused*/) {
    return codegen::StringParts{additional_args.name, "_"_sl, additional_args.depth};
}

template <StringLiteral element_name>
[[nodiscard]] constexpr auto get_unique_name (const GenFixedArrayLeafArgs& /*unused*/) {
    return element_name;
}

template <
    estd::invocable_r<
        estd::is_any_of<
            estd::is_same_meta<std::string_view>::type,
            codegen::is_string_parts_t
        >::type
    > F
>
requires(!std::is_reference_v<F>)
[[nodiscard]] constexpr auto get_unique_name (const GenFixedArrayLeafArgs& /*unused*/, F&& on_element) {
    return on_element();
}

template <StringLiteral element_name>
[[nodiscard]] constexpr auto get_unique_name (const GenArrayLeafArgs& /*unused*/) {
    return element_name;
}
template <
    estd::invocable_r<
        estd::is_any_of<
            estd::is_same_meta<std::string_view>::type,
            codegen::is_string_parts_t
        >::type
    > F
>
requires(!std::is_reference_v<F>)
[[nodiscard]] constexpr auto get_unique_name (const GenArrayLeafArgs& /*unused*/, F&& on_element) {
    return on_element();
}

[[nodiscard]] constexpr auto get_unique_name (const GenVariantLeafArgsBase& additional_args) {
    return codegen::StringParts{"as_"_sl, additional_args.variant_depth, "_"_sl, additional_args.variant_id};
}
template <StringLiteral element_name>
[[nodiscard]] constexpr auto get_unique_name (const GenVariantLeafArgsBase& additional_args) {
    return codegen::StringParts{"as_"_sl, additional_args.variant_depth, "_"_sl, additional_args.variant_id};
}
template <typename F>
[[nodiscard]] constexpr auto get_unique_name (const GenVariantLeafArgsBase& additional_args, F&& /*unused*/) {
    return codegen::StringParts{"as_"_sl, additional_args.variant_depth, "_"_sl, additional_args.variant_id};
}

[[nodiscard]] constexpr const std::string_view& get_name (const GenStructLeafArgs& additional_args) {
    return additional_args.name;
}
template <typename T>
[[nodiscard]] constexpr const std::string_view& get_name (const GenStructLeafArgs& additional_args, T /*unused*/) {
    return additional_args.name;
}

[[nodiscard]] constexpr auto get_name (const GenVariantLeafArgsBase& additional_args) {
    return codegen::StringParts{"as_"_sl, additional_args.variant_id};
}
template <typename T>
[[nodiscard]] constexpr auto get_name (const GenVariantLeafArgsBase& additional_args, T /*unused*/) {
    return codegen::StringParts{"as_"_sl, additional_args.variant_id};
}

template <LeafArgs ArgsT, typename T>
requires (!(std::is_same_v<ArgsT, GenStructLeafArgs> || std::is_same_v<ArgsT, GenVariantLeafArgsBase> || std::is_same_v<ArgsT, GenDynamicVariantLeafArgs>))
[[nodiscard]] constexpr T&& get_name (ArgsT /*unused*/, T&& unique_name) {
    return std::forward<T>(unique_name);
}

template <typename NameT>
requires (!LeafArgs<std::remove_cvref_t<NameT>>)
[[nodiscard]] constexpr NameT&& get_name (NameT&& name) {
    return std::forward<NameT>(name);
}


template <typename CodeT>
requires(!std::is_reference_v<CodeT>)
[[nodiscard]] inline CodeT&& add_size_leafs (
    std::span<SizeLeaf> level_size_leafs,
    const FixedOffset* const fixed_offsets,
    CodeT&& struct_code
) {
    for (size_t i = 0; i < level_size_leafs.size(); i++) {
        auto [min_size, idx, size_size, stored_size_size] = level_size_leafs[i];
        const FixedOffset& offset = fixed_offsets[idx];
        struct_code = struct_code
        .method(codegen::Attributes{"static"}, get_size_type_str(size_size), codegen::StringParts{"size", i}, codegen::Args{"size_t base"})
            .line("return *reinterpret_cast<", get_size_type_str(stored_size_size), "*>(base + ", offset.get_offset(), ");")
        .end();
    }
    return std::move(struct_code);
}

template <StringLiteral type_name, char target>
consteval bool _last_non_whitespace_is () {
    size_t i = type_name.size();
    char last_char = type_name.data[--i];
    while (last_char == ' ' || last_char == '\t') {
        last_char = type_name.data[--i];
    }
    return last_char == target;
}

template <StringLiteral type_name, char target>
constexpr bool last_non_whitespace_is = _last_non_whitespace_is<type_name, target>();

template <StringLiteral type_name, StringLiteral postfix>
consteval auto _get_first_return_line_part () {
    if constexpr (last_non_whitespace_is<type_name, '*'>) {
        return "return reinterpret_cast<"_sl + type_name + ">(base"_sl + postfix;
    } else {
        return "return *reinterpret_cast<"_sl + type_name + "*>(base"_sl + postfix;
    }
}

template <StringLiteral type_name, StringLiteral postfix = " + ">
constexpr auto get_first_return_line_part = _get_first_return_line_part<type_name, postfix>();

[[nodiscard]] constexpr uint64_t get_pack_size (const lexer::LeafSizes& sizes, const FixedOffset& fo) {
    return sizes.get(fo.get_pack_align());
}

[[nodiscard]] constexpr uint64_t get_pack_size (uint32_t size, const FixedOffset& /*unused*/) {
    return size;
}


template <bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename PackSizeT, typename Last>
[[nodiscard]] inline codegen::Method<Last>&& _gen_fixed_value_leaf_in_array (
    codegen::Method<Last>&& get_method,
    const OffsetsAccessor& offsets_accessor,
    const ArrayLengths& array_lengths,
    const PackSizeT& pack_size
) {
    const FixedOffset fo = offsets_accessor.next_fixed_leaf();
    const uint64_t offset = fo.get_offset();
    constexpr bool is_pack = !std::is_same_v<PackSizeT, estd::empty>;
    if constexpr (type_size == lexer::SIZE::SIZE_1 && !is_pack) {
        if (offset == 0) {
            get_method = get_method
            .line(get_first_return_line_part<type_name, "">, IdxCalcCodeGenerator<true, is_array_element>{array_lengths}, ");");
        } else {
            get_method = get_method
            .line(get_first_return_line_part<type_name>, offset, IdxCalcCodeGenerator<true, is_array_element>{array_lengths}, ");");
        }
    } else {
        if constexpr (is_pack) {
            if (offset == 0) {
                get_method = get_method
                .line(get_first_return_line_part<type_name, "">, IdxCalcCodeGenerator<false, is_array_element>{array_lengths}, " * ", get_pack_size(pack_size, fo), ");");
            } else {
                get_method = get_method
                .line(get_first_return_line_part<type_name>, offset, IdxCalcCodeGenerator<false, is_array_element>{array_lengths}, " * ", get_pack_size(pack_size, fo), ");");
            }
        } else {
            constexpr auto type_size_str = string_literal::from<type_size.byte_size()>;
            if (offset == 0) {
                get_method = get_method
                .line(get_first_return_line_part<type_name, "">, IdxCalcCodeGenerator<false, is_array_element>{array_lengths}, " * ", type_size_str, ");");
            } else {
                get_method = get_method
                .line(get_first_return_line_part<type_name>, offset, IdxCalcCodeGenerator<false, is_array_element>{array_lengths}, " * ", type_size_str, ");");
            }
        }
    }

    return std::move(get_method);
}

template <bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename Last>
[[nodiscard]] inline codegen::Method<Last>&& _gen_fixed_value_leaf_default (
    codegen::Method<Last>&& get_method,
    const OffsetsAccessor& offsets_accessor
) {
    const uint64_t offset = offsets_accessor.next_fixed_offset();

    if (offset == 0) {
        get_method = get_method
        .line(get_first_return_line_part<type_name, ");">);
    } else {
        get_method = get_method
        .line(get_first_return_line_part<type_name>, offset, ");");
    }

    return std::move(get_method);
}

template <bool in_array, bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename PackSizeT, typename ArgsT, typename CodeT>
requires(!std::is_reference_v<CodeT>)
[[nodiscard]] inline CodeT&& _gen_fixed_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    ArgsT&& name_providing_args,
    const ArrayLengths& array_lengths,
    const PackSizeT& pack_size
) {
    if constexpr (is_array_element) {
        auto&& get_method = code
        .method(type_name, "get", codegen::Args{"uint32_t idx"});
        
        get_method = _gen_fixed_value_leaf_in_array<true, type_name, type_size, PackSizeT>(std::move(get_method), offsets_accessor, array_lengths, pack_size);

        code = get_method
        .end();
    } else {
        auto&& get_method = code
        .method(type_name, get_name(std::forward<ArgsT>(name_providing_args)));
        if constexpr (in_array) {
            get_method = _gen_fixed_value_leaf_in_array<false, type_name, type_size, PackSizeT>(std::move(get_method), offsets_accessor, array_lengths, pack_size);
        } else {
            get_method = _gen_fixed_value_leaf_default<false, type_name, type_size>(std::move(get_method), offsets_accessor);
        }
        code = get_method
        .end();
    }
    return std::move(code);
}

template <bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename PackSizeT, typename Last>
[[nodiscard]] inline codegen::Method<Last>&& _gen_var_value_leaf_in_array (
    codegen::Method<Last>&& get_method,
    const OffsetsAccessor& offsets_accessor,
    const ArrayLengths& array_lengths,
    const PackSizeT& pack_size
) {
    const uint64_t& var_leafs_start = offsets_accessor.var_leafs_start;
    const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();
    constexpr bool is_pack = !std::is_same_v<PackSizeT, estd::empty>;

    if constexpr (type_size == lexer::SIZE::SIZE_1 && !is_pack) {
        if (size_chain.empty()) {
            get_method = get_method
            .line(get_first_return_line_part<type_name>, var_leafs_start, IdxCalcCodeGenerator<true, is_array_element>{array_lengths}, ");");
        } else {
            get_method = get_method
            .line(get_first_return_line_part<type_name>, var_leafs_start, SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, IdxCalcCodeGenerator<true, is_array_element>{array_lengths}, ");");
        }
    } else {
        if constexpr (std::is_same_v<PackSizeT, lexer::LeafSizes>) {
            BSSERT(false, "Variable sized leafs should not be part of pack (idk)");
        } else if constexpr (is_pack) {
            if (size_chain.empty()) {
                get_method = get_method
                .line(get_first_return_line_part<type_name>, var_leafs_start, IdxCalcCodeGenerator<false, is_array_element>{array_lengths}, " * ", pack_size, ");");
            } else {
                get_method = get_method
                .line(get_first_return_line_part<type_name>, var_leafs_start, SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, IdxCalcCodeGenerator<false, is_array_element>{array_lengths}, " * ", pack_size, ");");
            }
        } else {
            constexpr auto type_size_str = string_literal::from<type_size.byte_size()>;
            if (size_chain.empty()) {
                get_method = get_method
                .line(get_first_return_line_part<type_name>, var_leafs_start, IdxCalcCodeGenerator<false, is_array_element>{array_lengths}, " * ", type_size_str, ");");
            } else {
                get_method = get_method
                .line(get_first_return_line_part<type_name>, var_leafs_start, SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, IdxCalcCodeGenerator<false, is_array_element>{array_lengths}, " * ", type_size_str, ");");
            }
        }
    }
    return std::move(get_method);
}

template <bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename Last>
[[nodiscard]] inline codegen::Method<Last>&& _gen_var_value_leaf_default (
    codegen::Method<Last>&& get_method,
    const OffsetsAccessor& offsets_accessor
) {
    const uint64_t& var_leafs_start = offsets_accessor.var_leafs_start;
    const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();

    if (size_chain.empty()) {
        get_method = get_method
        .line(get_first_return_line_part<type_name>, var_leafs_start, ");");
    } else {
        get_method = get_method
        .line(get_first_return_line_part<type_name>, var_leafs_start, SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, ");");
    }

    return std::move(get_method);
}

template <bool in_array, bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, typename PackSizeT, typename ArgsT, typename CodeT>
requires(!std::is_reference_v<CodeT>)
[[nodiscard]] inline CodeT&& _gen_var_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    ArgsT&& name_providing_args,
    const ArrayLengths& array_lengths,
    const PackSizeT& pack_size
) {
    if constexpr (is_array_element) {
        auto&& get_method = code
        .method(type_name, "get", codegen::Args{"uint32_t idx"});

        get_method = _gen_var_value_leaf_in_array<true, type_name, type_size, PackSizeT>(std::move(get_method), offsets_accessor, array_lengths, pack_size);

        code = get_method
        .end();
    } else {
        auto&& get_method = code
        .method(type_name, get_name(std::forward<ArgsT>(name_providing_args)));
        if constexpr (in_array) {
            get_method = _gen_var_value_leaf_in_array<false, type_name, type_size, PackSizeT>(std::move(get_method), offsets_accessor, array_lengths, pack_size);
        } else {
            get_method = _gen_var_value_leaf_default<false, type_name, type_size>(std::move(get_method), offsets_accessor);
        }
        code = get_method
        .end();
    }
    return std::move(code);
}

template <bool is_fixed, bool is_array_element, bool in_array, StringLiteral type_name, lexer::SIZE type_size, typename PackSizeT, typename ArgsT, typename CodeT>
requires(!std::is_reference_v<CodeT>)
[[nodiscard]] inline CodeT&& _gen_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    ArgsT&& name_providing_args,
    const ArrayLengths& array_lengths,
    const PackSizeT& pack_size
) {
    if constexpr (is_fixed) {
        return _gen_fixed_value_leaf<in_array, is_array_element, type_name, type_size, PackSizeT>(std::move(code), offsets_accessor, std::forward<ArgsT>(name_providing_args), array_lengths, pack_size);
    } else {
        return _gen_var_value_leaf<in_array, is_array_element, type_name, type_size, PackSizeT>(std::move(code), offsets_accessor, std::forward<ArgsT>(name_providing_args), array_lengths, pack_size);
    }
}

template <bool is_fixed, bool in_array, StringLiteral type_name, lexer::SIZE type_size, typename PackSizeT = estd::empty, LeafArgs ArgsT, typename CodeT>
requires(!std::is_reference_v<CodeT>)
[[nodiscard]] inline CodeT&& gen_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    const ArgsT& additional_args,
    const ArrayLengths& array_lengths,
    const PackSizeT& pack_size = estd::empty{}
) {
    return _gen_value_leaf<is_fixed, is_array_element<ArgsT>, in_array, type_name, type_size, PackSizeT>(std::move(code), offsets_accessor, additional_args, array_lengths, pack_size);
}

template <bool is_fixed, bool is_array_element, bool in_array, StringLiteral type_name, lexer::SIZE type_size, typename PackSizeT = estd::empty, typename NameT, typename CodeT>
requires(!std::is_reference_v<CodeT>)
[[nodiscard]] inline CodeT&& gen_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    NameT&& name,
    const ArrayLengths& array_lengths,
    const PackSizeT& pack_size = estd::empty{}
) {
    return _gen_value_leaf<is_fixed, is_array_element, in_array, type_name, type_size, PackSizeT>(std::move(code), offsets_accessor, std::forward<NameT>(name), array_lengths, pack_size);
}

template <typename ArgsT, typename UniqueNameT, typename CodeT>
requires(!std::is_reference_v<CodeT>)
[[nodiscard]] inline CodeT&& gen_field_access_method_no_array (
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
struct TypeVisitor : lexer::TypeVisitorBase<TypeT, codegen::UnknownStructBase, codegen::UnknownStructBase&&> {
    constexpr TypeVisitor (
        const lexer::Type* const& field,
        const ReadOnlyBuffer& ast_buffer,
        BaseNameT&& base_name,
        const OffsetsAccessor& offsets_accessor,
        const std::span<SizeLeaf>& level_size_leafs,
        gsl::not_null<uint16_t*> current_size_leaf_idx,
        const ArgsT& additional_args,
        const ArrayLengths& array_lengths,
        const lexer::LeafSizes& pack_sizes
    ) :
    lexer::TypeVisitorBase<TypeT, codegen::UnknownStructBase, codegen::UnknownStructBase&&>{field},
    ast_buffer(ast_buffer),
    base_name(std::forward<BaseNameT>(base_name)),
    offsets_accessor(offsets_accessor),
    level_size_leafs(level_size_leafs),
    current_size_leaf_idx(current_size_leaf_idx),
    additional_args(additional_args),
    array_lengths(array_lengths),
    pack_sizes(pack_sizes)
    {}

    ReadOnlyBuffer ast_buffer;
    std::remove_cvref_t<BaseNameT> base_name;
    OffsetsAccessor offsets_accessor;
    std::span<SizeLeaf> level_size_leafs;
    gsl::not_null<uint16_t*> current_size_leaf_idx;
    ArgsT additional_args;
    ArrayLengths array_lengths;
    #define array_depth array_lengths.length
    lexer::LeafSizes pack_sizes;
    

    template <lexer::FIELD_TYPE field_type, StringLiteral type_name>
    [[nodiscard]] codegen::UnknownStructBase on_simple (codegen::UnknownStructBase&& code) const {
        constexpr lexer::SIZE alignment = lexer::get_type_alignment<field_type>();
        if (!pack_sizes.empty()) {
            return gen_value_leaf<is_fixed, in_array, type_name, alignment, lexer::LeafSizes>(std::move(code), offsets_accessor, additional_args, array_lengths, pack_sizes);
        } else {
            return gen_value_leaf<is_fixed, in_array, type_name, alignment>(std::move(code), offsets_accessor, additional_args, array_lengths);
        }
    }

    [[nodiscard]] codegen::UnknownStructBase on_bool    (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::BOOL   , "bool"    >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_uint8   (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::UINT8  , "uint8_t" >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_uint16  (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::UINT16 , "uint16_t">(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_uint32  (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::UINT32 , "uint32_t">(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_uint64  (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::UINT64 , "uint64_t">(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_int8    (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::INT8   , "int8_t"  >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_int16   (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::INT16  , "int16_t" >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_int32   (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::INT32  , "int32_t" >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_int64   (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::INT64  , "int64_t" >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_float32 (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::FLOAT32, "float"   >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_float64 (codegen::UnknownStructBase&& code) const override { return on_simple<lexer::FIELD_TYPE::FLOAT64, "double"  >(std::move(code)); }

    [[nodiscard]] codegen::UnknownStructBase on_fixed_string (codegen::UnknownStructBase&& code, const lexer::FixedStringType* const fixed_string_type) const override {
        const uint32_t length = fixed_string_type->length;
        const std::string size_type_str =  get_size_type_str(fixed_string_type->length_size);

        const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto unique_name = get_unique_name<"String">(additional_args);

        auto&& string_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        string_struct = gen_value_leaf<is_fixed, false, in_array, "char*", lexer::SIZE::SIZE_1, uint32_t>(
            std::move(string_struct),
            offsets_accessor,
            "c_str"_sl,
            array_lengths,
            length
        );

        string_struct = string_struct
            .method(codegen::Attributes{"constexpr"}, size_type_str, "size")
                .line("return ", length, ";")
            .end()
            .method(codegen::Attributes{"constexpr"}, size_type_str, "length")
                .line("return size() - 1;")
            .end()
            ._private()
            .field("size_t", "base");

        for (uint8_t i = 0; i < array_depth; i++) {
            string_struct = string_struct
            .field("uint32_t", codegen::StringParts{"idx_", i});
        }
        if constexpr (is_dynamic_variant_element<ArgsT>) {
            BSSERT(level_size_leafs.size() == 0, "Unexpected state");
        }
        
        code = string_struct
        .end();

        if constexpr (is_array_element<ArgsT>) {
            code = code
            .method(unique_name, "get", codegen::Args{"uint32_t idx"})
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            code = gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
        }
        return std::move(code);
    }

    [[nodiscard]] codegen::UnknownStructBase on_string (codegen::UnknownStructBase&& code, const lexer::StringType* const string_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Variable length strings in arrays are not supported");
        } else {
            const lexer::SIZE size_size = string_type->size_size;
            const lexer::SIZE stored_size_size = string_type->stored_size_size;
            const std::string size_type_str = get_size_type_str(size_size);
            const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();

            const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

            const uint16_t size_leaf_idx = (*current_size_leaf_idx)++;
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
                const uint64_t offset = offsets_accessor.next_fixed_offset();
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

    [[nodiscard]] TypeVisitor::ResultT on_fixed_array (codegen::UnknownStructBase&& code, const lexer::ArrayType* const fixed_array_type) const override {
        const uint32_t length = fixed_array_type->length;
        const std::string size_type_str = get_size_type_str(fixed_array_type->size_size);

        const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

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
            decltype(estd::conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name))
        >{
            fixed_array_type->inner_type(),
            ast_buffer,
            estd::conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name),
            offsets_accessor,
            level_size_leafs,
            current_size_leaf_idx,
            GenFixedArrayLeafArgs{depth},
            ArrayLengths{new_array_lengths, gsl::narrow_cast<uint8_t>(array_depth + 1)},
            pack_sizes
        }.visit(codegen::UnknownStructBase{std::move(array_struct)});

        array_struct = codegen::NestedStruct<codegen::UnknownStructBase>{std::move(result.value)}
            .method(codegen::Attributes{"constexpr"}, size_type_str, "length")
                .line("return ", length, ";")
            .end()
            ._private()
            .field("size_t", "base");

        for (uint8_t i = 0; i < array_depth; i++) {
            array_struct = array_struct
            .field("uint32_t", codegen::StringParts{"idx_", i});
        }
        if constexpr (is_dynamic_variant_element<ArgsT>) {
            BSSERT(level_size_leafs.size() == 0, "Unexpected state");
        }

        code = array_struct
        .end();
        
        if constexpr (is_array_element<ArgsT>) {
            code = code
            .method(unique_name, "get", codegen::Args{"uint32_t idx"})
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


    [[nodiscard]] TypeVisitor::ResultT on_array (codegen::UnknownStructBase&& code, const lexer::ArrayType* const array_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
            const lexer::SIZE size_size = array_type->size_size;
            const lexer::SIZE stored_size_size = array_type->stored_size_size;
            const std::string size_type_str = get_size_type_str(size_size);

            const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(0);

            auto unique_name = get_unique_name(additional_args);

            auto&& array_struct = code
            ._struct(unique_name)
                .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

            typename TypeVisitor::ResultT result = TypeVisitor<
                TypeT,
                false,
                true,
                GenArrayLeafArgs,
                decltype(estd::conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name))
            >{
                array_type->inner_type(),
                ast_buffer,
                estd::conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name),
                offsets_accessor,
                level_size_leafs,
                current_size_leaf_idx,
                GenArrayLeafArgs{},
                ArrayLengths{nullptr, 1},
                pack_sizes
            }.visit(codegen::UnknownStructBase{std::move(array_struct)});

            const uint16_t size_leaf_idx = (*current_size_leaf_idx)++;
            logger::debug("ARRAY size_leaf_idx:", size_leaf_idx);
            level_size_leafs[size_leaf_idx] = {
                array_type->length,
                offsets_accessor.next_map_idx(),
                size_size,
                stored_size_size
            };

            array_struct = codegen::NestedStruct<codegen::UnknownStructBase>{std::move(result.value)}
                .method(size_type_str, "length")
                    .line("return size", size_leaf_idx, "(base);")
                .end()
                ._private()
                .field("size_t", "base");
            if constexpr (is_dynamic_variant_element<ArgsT>) {
                array_struct = add_size_leafs(level_size_leafs, offsets_accessor.fixed_offsets, std::move(array_struct));
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

    [[nodiscard]] TypeVisitor::ResultT on_fixed_variant (codegen::UnknownStructBase&& code, lexer::FixedVariantType* const fixed_variant_type) const override {
        const uint16_t variant_count = fixed_variant_type->variant_count;

        auto unique_name = get_unique_name<"Variant">(additional_args);

        const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);
        
        auto&& variant_struct = code
        ._struct(unique_name);

        variant_struct = variant_struct
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        if (variant_count <= UINT8_MAX) {
            variant_struct = gen_value_leaf<is_fixed, false, in_array, "uint8_t", lexer::SIZE::SIZE_1>(std::move(variant_struct), offsets_accessor, "id"_sl, array_lengths);
        } else {
            variant_struct = gen_value_leaf<is_fixed, false, in_array, "uint16_t", lexer::SIZE::SIZE_2>(std::move(variant_struct), offsets_accessor, "id"_sl, array_lengths);
        }              

        const lexer::Type* type = fixed_variant_type->first_variant();
        // uint64_t max_offset = 0;

        uint16_t variant_depth;
        if constexpr (is_variant_element<ArgsT>) {
            variant_depth = additional_args.variant_depth + 1;
        } else {
            variant_depth = 0;
        }

        // uint64_t max_size = 12345; // TODO. figure out how to store this when genrating offsets.
        
        for (uint16_t i = 0; i < variant_count; i++) {
            // GenFixedVariantLeafArgs<in_array> gen_variant_leaf_args;
            // if constexpr (in_array) {
            //     gen_variant_leaf_args = GenFixedVariantLeafArgs<true>{{i, variant_depth}, max_size, idx_calc_str};
            // } else {
            //     gen_variant_leaf_args = GenFixedVariantLeafArgs<false>{{i, variant_depth}};
            // }
            
            lexer::TypeVisitorResult<lexer::Type, codegen::UnknownStructBase> result = TypeVisitor<
                lexer::Type,
                is_fixed,
                in_array,
                GenFixedVariantLeafArgs,
                decltype(estd::conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name))
            >{
                type,
                ast_buffer,
                estd::conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name),
                offsets_accessor,
                std::span<SizeLeaf>{},
                current_size_leaf_idx,
                GenFixedVariantLeafArgs{
                    {i, variant_depth}
                },
                array_lengths,
                fixed_variant_type->pack_sizes
            }.visit(codegen::UnknownStructBase{std::move(variant_struct)});

            type = result.next_type;
            variant_struct = codegen::NestedStruct<codegen::UnknownStructBase>{std::move(result.value)};
        }
        
        variant_struct = variant_struct
        ._private()
            .field("size_t", "base");

        for (uint8_t i = 0; i < array_depth; i++) {
            variant_struct = variant_struct
            .field("uint32_t", codegen::StringParts{"idx_", i});
        }
        if constexpr (is_dynamic_variant_element<ArgsT>) {
            variant_struct = add_size_leafs(level_size_leafs, offsets_accessor.fixed_offsets, std::move(variant_struct));
        }

        code = variant_struct
        .end();

        

        if constexpr (is_array_element<ArgsT>) {
            code = code
            .method("Variant", "get", codegen::Args{"uint32_t idx"})
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            code = gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
        }

        return {
            reinterpret_cast<TypeVisitor::ConstTypeT*>(type),
            std::move(code)
        };
    }

    [[nodiscard]] TypeVisitor::ResultT on_packed_variant (codegen::UnknownStructBase&&  /*unused*/, const lexer::PackedVariantType* const /*unused*/) const override {
        INTERNAL_ERROR("Packed variant not supported");
    }

    [[nodiscard]] TypeVisitor::ResultT on_dynamic_variant (codegen::UnknownStructBase&& code, const lexer::DynamicVariantType* const dynamic_variant_type) const override {
        if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
            const uint16_t variant_count = dynamic_variant_type->variant_count;

            auto unique_name = get_unique_name<"DynamicVariant">(additional_args);

            const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);
            
            auto&& variant_struct = code
            ._struct(unique_name)
                .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();
            
            if (variant_count <= UINT8_MAX) {
                variant_struct = gen_value_leaf<is_fixed, false, in_array, "uint8_t", lexer::SIZE::SIZE_1>(std::move(variant_struct), offsets_accessor, "id"_sl, array_lengths);
            } else {
                variant_struct = gen_value_leaf<is_fixed, false, in_array, "uint16_t", lexer::SIZE::SIZE_2>(std::move(variant_struct), offsets_accessor, "id"_sl, array_lengths);
            }

            const uint16_t size_leaf_idx = (*current_size_leaf_idx)++;
            logger::debug("ARRAY size_leaf_idx: ", size_leaf_idx);
            level_size_leafs[size_leaf_idx] = {
                dynamic_variant_type->min_byte_size,
                offsets_accessor.next_map_idx(),
                dynamic_variant_type->size_size,
                dynamic_variant_type->stored_size_size
            };

            const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();
            
            std::string_view offset;
            if (!size_chain.empty()) {
                const SizeChainCodeGenerator generator {
                    offsets_accessor.var_offset_buffer,
                    size_chain
                };
                size_t buf_size = generator.get_size();
                char* buf = static_cast<char*>(alloca(buf_size));
                char* end = generator.write(buf);
                auto d = end - buf;
                BSSERT(d >= 0 && buf_size == gsl::narrow_cast<size_t>(d), " ", buf_size, " == ", reinterpret_cast<uintptr_t>(end), " - ", reinterpret_cast<uintptr_t>(buf));
                offset = {buf, buf_size};
            }

            const lexer::Type* type = dynamic_variant_type->first_variant();

            uint16_t variant_depth;
            if constexpr (is_variant_element<ArgsT>) {
                variant_depth = additional_args.variant_depth + 1;
            } else {
                variant_depth = 0;
            }

            // uint64_t max_size = 12345; // TODO. figure out how to store this when genrating offsets.
            
            for (uint16_t i = 0; i < variant_count; i++) {
                const auto& type_meta = dynamic_variant_type->type_metas()[i];
                const auto level_size_leafs_count = type_meta.level_size_leafs;

                SizeLeaf level_size_leafs[level_size_leafs_count];

                // GenFixedVariantLeafArgs<in_array> gen_variant_leaf_args;
                // if constexpr (in_array) {
                //     gen_variant_leaf_args = GenFixedVariantLeafArgs<true>{{i, variant_depth}, max_size, idx_calc_str};
                // } else {
                //     gen_variant_leaf_args = GenFixedVariantLeafArgs<false>{{i, variant_depth}};
                // }
                uint16_t current_size_leaf_idx = 0;
                lexer::TypeVisitorResult<TypeT, codegen::UnknownStructBase> result = TypeVisitor<
                    TypeT,
                    true,
                    in_array,
                    GenDynamicVariantLeafArgs,
                    decltype(estd::conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name))
                >{
                    type,
                    ast_buffer,
                    estd::conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name),
                    offsets_accessor,
                    std::span<SizeLeaf>{level_size_leafs, level_size_leafs + level_size_leafs_count},
                    &current_size_leaf_idx,
                    GenDynamicVariantLeafArgs{
                        offset,
                        {i, variant_depth}
                    },
                    array_lengths,
                    pack_sizes
                }.visit(codegen::UnknownStructBase{std::move(variant_struct)});
                type = (lexer::Type*)result.next_type;
                variant_struct = codegen::NestedStruct<codegen::UnknownStructBase>{std::move(result.value)};
            }

            variant_struct = variant_struct
            ._private()
                .field("size_t", "base");

            for (uint8_t i = 0; i < array_depth; i++) {
                variant_struct = variant_struct
                .field("uint32_t", codegen::StringParts{"idx_", i});
            }
            if constexpr (is_dynamic_variant_element<ArgsT>) {
                logger::debug("Adding size leafs, variant_depth: ", additional_args.variant_depth);
                variant_struct = add_size_leafs(level_size_leafs, offsets_accessor.fixed_offsets, std::move(variant_struct));
            }
            code = variant_struct
            .end();

            if constexpr (is_array_element<ArgsT>) {
                static_assert(false, "Dynamic array cant be array element");
            } else {
                if constexpr (!in_array) {
                    code = gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
                } else {
                    static_assert(false, "Dynamic array cant be in array");
                }
            }

            
            return {
                reinterpret_cast<typename TypeVisitor::ConstTypeT*>(type),
                std::move(code)
            };
        }
    }

    [[nodiscard]] codegen::UnknownStructBase on_identifier (codegen::UnknownStructBase&& code, const lexer::IdentifiedType* const identified_type) const override {
        const auto* const identifier = ast_buffer.get(identified_type->identifier_idx);
        if (identifier->keyword != lexer::KEYWORDS::STRUCT) {
            INTERNAL_ERROR("not implemented");
        }
        const auto* const struct_type = identifier->data()->as_struct();

        const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto unique_name = get_unique_name(additional_args, [&struct_type]() { return struct_type->name; });
        
        auto&& struct_code = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        const auto* field = struct_type->first_field();
        for (uint16_t i = 0; i < struct_type->field_count; i++) {
            const auto* const field_data = field->data();
            uint16_t struct_depth;
            if constexpr (std::is_same_v<ArgsT, GenStructLeafArgs>) {
                struct_depth = additional_args.depth + 1;
            } else {
                struct_depth = 0;
            }

            lexer::TypeVisitorResult<lexer::StructField, codegen::UnknownStructBase> result = TypeVisitor<
                lexer::StructField,
                is_fixed,
                in_array,
                GenStructLeafArgs,
                decltype(estd::conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name))
            >{
                field_data->type(),
                ast_buffer,
                estd::conditionally<is_dynamic_variant_element<ArgsT>>(unique_name, base_name),
                offsets_accessor,
                level_size_leafs,
                current_size_leaf_idx,
                GenStructLeafArgs{field_data->name, struct_depth},
                array_lengths,
                pack_sizes
            }.visit(codegen::UnknownStructBase{std::move(struct_code)});
        
            field = result.next_type;
            struct_code = codegen::NestedStruct<codegen::UnknownStructBase>{std::move(result.value)};
        }

        struct_code = struct_code
            ._private()
            .field("size_t", "base");
        
        for (uint8_t i = 0; i < array_depth; i++) {
            struct_code = struct_code
            .field("uint32_t", codegen::StringParts{"idx_", i});
        }
        if constexpr (is_dynamic_variant_element<ArgsT>) {
            struct_code = add_size_leafs(level_size_leafs, offsets_accessor.fixed_offsets, std::move(struct_code));
        }

        code = struct_code
        .end();

        if constexpr (is_array_element<ArgsT>) {
            code = code
            .method(unique_name, "get", codegen::Args{"uint32_t idx"})
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

void print_leafs (std::string_view name, lexer::LeafCounts::Counts counts) {
    logger::debug(name, ": {8:", counts.align1, ", 16:", counts.align2, ", 32:", counts.align4, ", 64:", counts.align8, ", total:", counts.total(), "}");
}

void generate (
    const lexer::StructDefinition* target_struct,
    const ReadOnlyBuffer& ast_buffer,
    const int output_fd
) {
    auto code = codegen::create_code(BUFFER_INIT_STACK(1 << 14))
    .line("#include \"lib/lib.hpp\"")
    .line("");
    const lexer::LeafCounts::Counts fixed_leaf_counts = target_struct->fixed_leaf_counts.counts;
    const lexer::LeafCounts::Counts var_leaf_counts = target_struct->var_leaf_counts.counts;
    const uint16_t level_fixed_variants = target_struct->level_fixed_variants;
    const uint16_t total_fixed_leafs = fixed_leaf_counts.total();
    const uint16_t total_var_leafs = var_leaf_counts.total();
    const uint16_t total_variant_fixed_leafs = target_struct->total_variant_fixed_leafs;
    const uint16_t total_variant_var_leafs = target_struct->total_variant_var_leafs;
    const uint16_t total_leafs = total_fixed_leafs + total_var_leafs + total_variant_fixed_leafs  + total_variant_var_leafs;
    const uint16_t level_size_leafs_count = target_struct->level_size_leafs;
    print_leafs("fixed_leaf_counts", fixed_leaf_counts);
    print_leafs("var_leaf_counts", var_leaf_counts);
    logger::debug("level_fixed_variants: ", level_fixed_variants);
    logger::debug("level_variant_fields: ", target_struct->level_variant_fields, " vs ", level_fixed_variants);
    logger::debug("total_variant_fixed_leafs: ", total_variant_fixed_leafs);
    logger::debug("total_variant_var_leafs: ", total_variant_var_leafs);
    logger::debug("total_leafs: ", total_leafs);
    logger::debug("level_size_leafs: ", level_size_leafs_count);

    std::string_view struct_name = target_struct->name;
    logger::debug("fixed_offsets length: ", total_fixed_leafs + total_variant_fixed_leafs);
    FixedOffset fixed_offsets[total_fixed_leafs + total_variant_fixed_leafs];
    logger::debug("fixed_offsets ptr: ", size_t(fixed_offsets));
    logger::debug("var_offsets length: ", total_var_leafs + total_variant_var_leafs);
    Buffer::View<uint64_t> var_offsets[total_var_leafs + total_variant_var_leafs];
    logger::debug("idx_map length: ", total_leafs);
    uint16_t idx_map[total_leafs];

    Buffer var_offset_buffer = BUFFER_INIT_STACK(sizeof(uint64_t) * 512);

    #define DO_OFFSET_GEN_BENCHMARK 0
    #if DO_OFFSET_GEN_BENCHMARK
    for (size_t i = 0; i < 10'000'000; ++i) {
        auto generate_offsets_result = generate_offsets::generate(
            target_struct,
            generate_offsets::TypeVisitorState::ConstState{
                ast_buffer,
                fixed_offsets,
                var_offsets,
                idx_map
            },
            std::move(var_offset_buffer),
            fixed_leaf_counts,
            var_leaf_counts,
            variant_field_counts,
            total_fixed_leafs,
            total_var_leafs,
            level_variant_fields,
            level_size_leafs_count
        );
        var_offset_buffer = std::move(generate_offsets_result.var_offset_buffer);
        var_offset_buffer.clear();
    }
    #endif
    #undef DO_OFFSET_GEN_BENCHMARK
    

    auto generate_offsets_result = generate_offsets::generate(
        target_struct,
        generate_offsets::TypeVisitorState::ConstState{
            ast_buffer,
            fixed_offsets,
            var_offsets,
            idx_map
        },
        std::move(var_offset_buffer),
        fixed_leaf_counts,
        var_leaf_counts,
        total_fixed_leafs,
        total_var_leafs,
        level_fixed_variants,
        level_size_leafs_count
    );
    var_offset_buffer = std::move(generate_offsets_result.var_offset_buffer);

    uint16_t current_map_idx = 0;
    uint16_t current_variant_field_idx = 0;
    OffsetsAccessor offsets_accessor = {
        fixed_offsets,
        var_offsets,
        idx_map,
        ReadOnlyBuffer{var_offset_buffer},
        generate_offsets_result.var_leafs_start,
        &current_map_idx,
        &current_variant_field_idx
    };

    SizeLeaf _level_size_leafs[level_size_leafs_count];
    uint16_t current_size_leaf_idx = 0;

    std::span<SizeLeaf> level_size_leafs = {_level_size_leafs, _level_size_leafs + level_size_leafs_count};
    
    
    auto&& struct_code = code
    ._struct(struct_name)
        .ctor("size_t base", "base(base)").end();

    const auto* field = target_struct->first_field();
    for (uint16_t i = 0; i < target_struct->field_count; i++) {
        const auto* const field_data = field->data();
        auto name = field_data->name;
        auto result = TypeVisitor<
            lexer::StructField,
            true,
            false,
            GenStructLeafArgs,
            decltype(std::forward<decltype(struct_name)>(struct_name))
        >{
            field_data->type(),
            ast_buffer,
            std::forward<decltype(struct_name)>(struct_name),
            offsets_accessor,
            level_size_leafs,
            &current_size_leaf_idx,
            GenStructLeafArgs{name, 0},
            ArrayLengths{nullptr, 0},
            lexer::LeafSizes::zero()
        }.visit(codegen::UnknownStructBase{std::move(struct_code)});

        field = result.next_type;
        struct_code = std::remove_reference_t<decltype(struct_code)>::Derived_{std::move(result.value)};
        
    }

    struct_code = struct_code
        ._private()
        .field("size_t", "base");

    struct_code = add_size_leafs(level_size_leafs, fixed_offsets, std::move(struct_code));

    auto&& code_done = struct_code
    .end()
    .end();

    #define DO_WRITE_OUTPUT 1
    #if DO_WRITE_OUTPUT
    auto write_result = ::write(output_fd, code_done.data(), code_done.size());
    if (write_result == -1) {
        std::perror("write failed");
        std::exit(1);
    }
    #endif
    #undef DO_WRITE_OUTPUT

}


}