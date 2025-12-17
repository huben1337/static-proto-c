#pragma once

#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <gsl/pointers>
#include <gsl/util>
#include <memory>
#include <span>
#include <array>
#include <string_view>
#include <type_traits>
#include <utility>

#include "./codegen.hpp"
#include "./estd/concepts.hpp"
#include "./parser/lexer_types.hpp"
#include "./container/memory.hpp"
#include "./util/string_literal.hpp"
#include "./util/logger.hpp"
#include "./helper/internal_error.hpp"
#include "./helper/alloca.hpp"
#include "./estd/meta.hpp"
#include "./fast_math/sum_of_digits.hpp"
#include "./fast_math/log.hpp"
#include "./code_gen_stuff.hpp"
#include "./generate_offsets.hpp"
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
        std::span<const FixedOffset> fixed_offsets,
        std::span<const Buffer::View<uint64_t>> var_offsets,
        std::span<const uint16_t> idx_map,
        std::span<const ArrayPackInfo> pack_infos,
        ReadOnlyBuffer var_offset_buffer,
        uint64_t var_leafs_start,
        gsl::not_null<uint16_t*> current_map_idx,
        gsl::not_null<uint16_t*> current_variant_field_idx
    ) :
    fixed_offsets(fixed_offsets),
    var_offsets(var_offsets),
    idx_map(idx_map),
    pack_infos(pack_infos),
    var_offset_buffer(var_offset_buffer),
    var_leafs_start(var_leafs_start),
    current_map_idx(current_map_idx),
    current_variant_field_idx(current_variant_field_idx)
    {}
    std::span<const FixedOffset> fixed_offsets;
    std::span<const Buffer::View<uint64_t>> var_offsets;
    std::span<const uint16_t> idx_map;
    std::span<const ArrayPackInfo> pack_infos;
    ReadOnlyBuffer var_offset_buffer;
    uint64_t var_leafs_start;
    gsl::not_null<uint16_t*> current_map_idx;
    gsl::not_null<uint16_t*> current_variant_field_idx;

    [[nodiscard]] uint16_t next_map_idx () const {
        const uint16_t map_idx = (*current_map_idx)++;
        // console.debug("next_map_idx: ", map_idx);
        const uint16_t idx = idx_map[map_idx];
        BSSERT(idx != static_cast<uint16_t>(-1), "next_map_idx: ", map_idx);
        return idx;
    }

    [[nodiscard]] uint16_t next_variant_field_idx () const {
        uint16_t variant_field_idx = (*current_variant_field_idx)++;
        console.debug("next_variant_field_idx: ", variant_field_idx);
        return variant_field_idx;
    }

    [[nodiscard]] uint64_t next_fixed_offset () const {
        return next_fixed_leaf().offset;
    }

    [[nodiscard]] FixedOffset next_fixed_leaf () const {
        const FixedOffset offset = fixed_offsets[next_map_idx()];
        BSSERT(offset != FixedOffset::empty());
        return offset;
    }

    [[nodiscard]] Buffer::View<uint64_t> next_var_offset () const {
        const uint16_t idx = next_map_idx();
        const Buffer::View<uint64_t> offset = var_offsets[idx];
        BSSERT(offset.start_idx.value != static_cast<Buffer::index_t>(-1));
        // console.debug("next_var_offset at: ", idx, ", start_idx: ", offset.start_idx.value, ", length: ", offset.length);
        return offset;
    }
};

[[nodiscard]] constexpr std::string_view get_size_type_str (lexer::SIZE size) {
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
private:
    std::span<const ArrayPackInfo> pack_infos;
    ArrayPackInfo pack_info;
    Buffer::index_t estimated_size;
    uint8_t array_depth;

    [[nodiscard]] static Buffer::index_t estimate_size (const uint8_t array_depth) {
        BSSERT(array_depth > 0);
        if (array_depth == 1) {
            if constexpr (last_is_direct) {
                return " + idx"_sl.size();
            } else {
                return " + idx_0"_sl.size();
            }
        }

        Buffer::index_t size;

        if constexpr (last_is_direct) {
            size = ((array_depth - 1) * (" + idx_"_sl.size() + " * "_sl.size() + 19)) + "idx"_sl.size() + fast_math::sum_of_digits_unsafe(gsl::narrow_cast<uint8_t>(array_depth - 1));
        } else {
            size = (array_depth * " + idx_"_sl.size()) + ((array_depth - 1) * (" * "_sl.size() + 19)) + fast_math::sum_of_digits_unsafe(array_depth);
        }

        if constexpr (!no_multiply) {
            size += "("_sl.size() + ")"_sl.size();
        }

        return size;
    }

    IdxCalcCodeGenerator (const std::span<const ArrayPackInfo>& pack_infos, const ArrayPackInfo& pack_info, const uint8_t array_depth)
        : pack_infos(pack_infos),
        pack_info(pack_info),
        estimated_size(estimate_size(array_depth)),
        array_depth(array_depth) {}

public:
    IdxCalcCodeGenerator (const std::span<const ArrayPackInfo>& pack_infos, const uint16_t pack_info_idx, const uint8_t array_depth)
        : IdxCalcCodeGenerator{pack_infos, pack_infos[pack_info_idx], array_depth} {}
    
    WriteResult write (char* dst) const override {
        char* const start = dst;
        if (array_depth == 1) {
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

            ArrayPackInfo last_pack_info = pack_info;
    
            for (uint32_t i = 0; i < array_depth - 1; i++) {
                dst = stringify::write_string(dst, " + idx_");
                dst = stringify::write_string(dst, i);
                dst = stringify::write_string(dst, " * ");
                dst = stringify::write_string(dst, last_pack_info.size);
                last_pack_info = last_pack_info.get_parent(pack_infos);
            }

            if constexpr (last_is_direct) {
                dst = stringify::write_string(dst, " + idx");
            } else {
                dst = stringify::write_string(dst, " + idx_");
                dst = stringify::write_string(dst, array_depth - 1);
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
    const std::span<SizeLeaf> level_size_leafs,
    const std::span<const FixedOffset> fixed_offsets,
    CodeT&& struct_code
) {
    for (size_t i = 0; i < level_size_leafs.size(); i++) {
        auto [min_size, idx, size_size, stored_size_size] = level_size_leafs[i];
        const FixedOffset& offset = fixed_offsets[idx];
        struct_code = std::move(struct_code)
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

template <bool is_direct_pack>
using direct_pack_legnth_arg_t = std::conditional_t<is_direct_pack, const uint32_t, estd::empty>;

template <bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, bool is_direct_pack, typename Last>
[[nodiscard]] inline Last _gen_fixed_value_leaf_in_array (
    codegen::Method<Last>&& get_method,
    const OffsetsAccessor& offsets_accessor,
    const uint16_t pack_info_idx,
    const uint8_t array_depth,
    direct_pack_legnth_arg_t<is_direct_pack> direct_pack_length
) {
    const FixedOffset fo = offsets_accessor.next_fixed_leaf();
    const uint64_t offset = fo.get_offset();
    if constexpr (type_size == lexer::SIZE::SIZE_1 && !is_direct_pack) {
        if (offset == 0) {
            return std::move(get_method)
            .line(get_first_return_line_part<type_name, "">, IdxCalcCodeGenerator<true, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, ");")
            .end();
        } else {
            return std::move(get_method)
            .line(get_first_return_line_part<type_name>, offset, IdxCalcCodeGenerator<true, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, ");")
            .end();
        }
    } else {
        if constexpr (is_direct_pack) {
            constexpr auto type_byte_size = type_size.byte_size();
            if (offset == 0) {
                return std::move(get_method)
                .line(get_first_return_line_part<type_name, "">, IdxCalcCodeGenerator<false, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, " * ", type_byte_size * direct_pack_length, ");")
                .end();
            } else {
                return std::move(get_method)
                .line(get_first_return_line_part<type_name>, offset, IdxCalcCodeGenerator<false, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, " * ", type_byte_size * direct_pack_length, ");")
                .end();
            }
        } else {
            constexpr auto type_size_str = string_literal::from<type_size.byte_size()>;
            if (offset == 0) {
                return std::move(get_method)
                .line(get_first_return_line_part<type_name, "">, IdxCalcCodeGenerator<false, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, " * "_sl + type_size_str + ");"_sl)
                .end();
            } else {
                return std::move(get_method)
                .line(get_first_return_line_part<type_name>, offset, IdxCalcCodeGenerator<false, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, " * "_sl + type_size_str + ");"_sl)
                .end();
            }
        }
    }
}

template <bool is_fixed, bool is_array_element, bool in_array, StringLiteral type_name, lexer::SIZE type_size, bool is_direct_pack = false, typename ArgsT, typename CodeT>
requires(is_fixed && !std::is_reference_v<CodeT>)
[[nodiscard]] inline CodeT gen_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    ArgsT&& name_providing_args,
    const uint16_t pack_info_idx,
    const uint8_t array_depth,
    direct_pack_legnth_arg_t<is_direct_pack> direct_pack_length = estd::empty{}
) {
    if constexpr (is_array_element) {      
        return _gen_fixed_value_leaf_in_array<true, type_name, type_size, is_direct_pack>(
            std::move(code)
                .method(type_name, "get", codegen::Args{"uint32_t idx"}),
            offsets_accessor,
            pack_info_idx,
            array_depth,
            direct_pack_length
        );
    } else {
        auto&& get_method = std::move(code)
            .method(type_name, get_name(std::forward<ArgsT>(name_providing_args)));
        if constexpr (in_array) {
            return _gen_fixed_value_leaf_in_array<false, type_name, type_size, is_direct_pack>(std::move(get_method), offsets_accessor, pack_info_idx, array_depth, direct_pack_length);
        } else {
            // console.warn("direct_pack_length not used. is that fine?");
            // _gen_fixed_value_leaf_default
            const uint64_t offset = offsets_accessor.next_fixed_offset();
            if (offset == 0) {
                return std::move(get_method)
                    .line(get_first_return_line_part<type_name, ");">)
                    .end();
            } else {
                return std::move(get_method)
                    .line(get_first_return_line_part<type_name>, offset, ");")
                    .end();
            }
        }
    }
}

template <bool is_array_element, StringLiteral type_name, lexer::SIZE type_size, bool is_direct_pack, typename Last>
[[nodiscard]] inline Last _gen_var_value_leaf_in_array (
    codegen::Method<Last>&& get_method,
    const OffsetsAccessor& offsets_accessor,
    const uint16_t pack_info_idx,
    const uint8_t array_depth,
    direct_pack_legnth_arg_t<is_direct_pack> direct_pack_length
) {
    const uint64_t& var_leafs_start = offsets_accessor.var_leafs_start;
    const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();

    if constexpr (type_size == lexer::SIZE::SIZE_1 && !is_direct_pack) {
        if (size_chain.empty()) {
            return std::move(get_method)
            .line(get_first_return_line_part<type_name>, var_leafs_start, IdxCalcCodeGenerator<true, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, ");")
            .end();
        } else {
            return std::move(get_method)
            .line(get_first_return_line_part<type_name>, var_leafs_start, SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, IdxCalcCodeGenerator<true, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, ");")
            .end();
        }
    } else {
        if constexpr (is_direct_pack) {
            constexpr auto type_byte_size = type_size.byte_size();
            if (size_chain.empty()) {
                return std::move(get_method)
                .line(get_first_return_line_part<type_name>, var_leafs_start, IdxCalcCodeGenerator<false, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, " * ", type_byte_size * direct_pack_length, ");")
                .end();
            } else {
                return std::move(get_method)
                .line(get_first_return_line_part<type_name>, var_leafs_start, SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, IdxCalcCodeGenerator<false, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, " * ", type_byte_size * direct_pack_length, ");")
                .end();
            }
        } else {
            constexpr auto type_size_str = string_literal::from<type_size.byte_size()>;
            if (size_chain.empty()) {
                return std::move(get_method)
                .line(get_first_return_line_part<type_name>, var_leafs_start, IdxCalcCodeGenerator<false, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, " * "_sl + type_size_str + ");"_sl)
                .end();
            } else {
                return std::move(get_method)
                .line(get_first_return_line_part<type_name>, var_leafs_start, SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, IdxCalcCodeGenerator<false, is_array_element>{offsets_accessor.pack_infos, pack_info_idx, array_depth}, " * "_sl + type_size_str + ");"_sl)
                .end();
            }
        }
    }
}

template <bool is_fixed, bool is_array_element, bool in_array, StringLiteral type_name, lexer::SIZE type_size, bool is_direct_pack = false, typename ArgsT, typename CodeT>
requires(!is_fixed && !std::is_reference_v<CodeT>)
[[nodiscard]] inline CodeT gen_value_leaf (
    CodeT&& code,
    const OffsetsAccessor& offsets_accessor,
    ArgsT&& name_providing_args,
    const uint16_t pack_info_idx,
    const uint8_t array_depth,
    direct_pack_legnth_arg_t<is_direct_pack> direct_pack_length = estd::empty{}
) {
    if constexpr (is_array_element) {
        return _gen_var_value_leaf_in_array<true, type_name, type_size, is_direct_pack>(
            std::move(code)
                .method(type_name, "get", codegen::Args{"uint32_t idx"}),
            offsets_accessor,
            pack_info_idx,
            array_depth,
            direct_pack_length
        );
    } else {
        auto&& get_method = std::move(code)
            .method(type_name, get_name(std::forward<ArgsT>(name_providing_args)));
        if constexpr (in_array) {
            return _gen_var_value_leaf_in_array<false, type_name, type_size, is_direct_pack>(
                std::move(get_method), offsets_accessor, pack_info_idx, array_depth, direct_pack_length);
        } else {
            static_assert(!is_direct_pack);
            // _gen_var_value_leaf_default
            const uint64_t& var_leafs_start = offsets_accessor.var_leafs_start;
            const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();

            if (size_chain.empty()) {
                return std::move(get_method)
                    .line(get_first_return_line_part<type_name>, var_leafs_start, ");")
                    .end();
            } else {
                return std::move(get_method)
                    .line(get_first_return_line_part<type_name>, var_leafs_start, SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, ");")
                    .end();
            }
        }
    }
}

template <typename ArgsT, typename UniqueNameT, typename CodeT>
requires(!std::is_reference_v<CodeT>)
[[nodiscard]] inline CodeT gen_field_access_method_no_array (
    CodeT&& code,
    const ArgsT& additional_args,
    const std::string_view& ctor_used,
    UniqueNameT&& unique_name
) {
    auto&& field_method = std::move(code)
    .method(unique_name, get_name(additional_args, unique_name));
    if constexpr (is_dynamic_variant_element<ArgsT>) {
        if (additional_args.offset.empty()) {
            // console.debug("[gen_field_access_method_no_array] additional_args.offset is empty");
            field_method = std::move(field_method)
            .line(ctor_used);
        } else {
            field_method = std::move(field_method)
            .line("return {base", additional_args.offset, std::string_view{ctor_used.data() + 12, ctor_used.size() - 12});
        }
    } else {
        field_method = std::move(field_method)
        .line(ctor_used);
    }
    return std::move(field_method)
        .end();
}

template <typename NextTypeT, bool is_fixed, bool in_array, typename Args, typename BaseNameArg>
struct TypeVisitor {
    constexpr TypeVisitor (
        const ReadOnlyBuffer ast_buffer,
        BaseNameArg base_name,
        const OffsetsAccessor offsets_accessor,
        const std::span<SizeLeaf> level_size_leafs,
        const gsl::not_null<uint16_t*> current_size_leaf_idx,
        const Args& additional_args,
        const uint8_t array_depth,
        const lexer::LeafSizes pack_sizes
    )
        : ast_buffer(ast_buffer),
        base_name(std::forward<BaseNameArg>(base_name)),
        offsets_accessor(offsets_accessor),
        level_size_leafs(level_size_leafs),
        current_size_leaf_idx(current_size_leaf_idx),
        additional_args(additional_args),
        array_depth(array_depth),
        pack_sizes(pack_sizes) {}

    using next_type_t = NextTypeT;
    using result_t = lexer::Type::VisitResult<next_type_t, codegen::UnknownStructBase>;

    ReadOnlyBuffer ast_buffer;
    std::remove_cvref_t<BaseNameArg> base_name;
    OffsetsAccessor offsets_accessor;
    std::span<SizeLeaf> level_size_leafs;
    gsl::not_null<uint16_t*> current_size_leaf_idx;
    Args additional_args;
    uint8_t array_depth;
    lexer::LeafSizes pack_sizes;
    uint16_t pack_info_idx = 0;
    

    template <lexer::FIELD_TYPE field_type, StringLiteral type_name>
    [[nodiscard]] codegen::UnknownStructBase on_simple (codegen::UnknownStructBase&& code) const {
        constexpr lexer::SIZE alignment = lexer::get_type_alignment<field_type>();
        return gen_value_leaf<is_fixed, is_array_element<Args>, in_array, type_name, alignment>(std::move(code), offsets_accessor, additional_args, pack_info_idx, array_depth);
       
    }

    [[nodiscard]] codegen::UnknownStructBase on_bool    (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::BOOL   , "bool"    >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_uint8   (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::UINT8  , "uint8_t" >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_uint16  (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::UINT16 , "uint16_t">(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_uint32  (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::UINT32 , "uint32_t">(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_uint64  (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::UINT64 , "uint64_t">(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_int8    (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::INT8   , "int8_t"  >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_int16   (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::INT16  , "int16_t" >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_int32   (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::INT32  , "int32_t" >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_int64   (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::INT64  , "int64_t" >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_float32 (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::FLOAT32, "float"   >(std::move(code)); }
    [[nodiscard]] codegen::UnknownStructBase on_float64 (codegen::UnknownStructBase&& code) const { return on_simple<lexer::FIELD_TYPE::FLOAT64, "double"  >(std::move(code)); }

    [[nodiscard]] codegen::UnknownStructBase on_fixed_string (const lexer::FixedStringType* const fixed_string_type, codegen::UnknownStructBase&& code) const {
        const uint32_t length = fixed_string_type->length;
        const std::string_view size_type_str =  get_size_type_str(fixed_string_type->length_size);

        const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto unique_name = get_unique_name<"String">(additional_args);

        auto&& string_struct = gen_value_leaf<is_fixed, false, in_array, "char*", lexer::SIZE::SIZE_1, true>(
            std::move(code)
                ._struct(unique_name)
                    .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end(),
            offsets_accessor,
            "c_str"_sl,
            pack_info_idx,
            array_depth,
            length
        )
            .method(codegen::Attributes{"constexpr"}, size_type_str, "size")
                .line("return ", length, ";")
            .end()
            .method(codegen::Attributes{"constexpr"}, size_type_str, "length")
                .line("return size() - 1;")
            .end()
            ._private()
            .field("size_t", "base");

        for (uint8_t i = 0; i < array_depth; i++) {
            string_struct = std::move(string_struct)
            .field("uint32_t", codegen::StringParts{"idx_", i});
        }
        if constexpr (is_dynamic_variant_element<Args>) {
            BSSERT(level_size_leafs.size() == 0, "Unexpected state");
        }
        
        code = std::move(string_struct)
        .end();

        if constexpr (is_array_element<Args>) {
            return std::move(code)
                .method(unique_name, "get", codegen::Args{"uint32_t idx"})
                .line(array_ctor_strs.el_ctor_used)
                .end();
        } else {
            return gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
        }
    }

    [[nodiscard]] codegen::UnknownStructBase on_string (const lexer::StringType* const string_type, codegen::UnknownStructBase&& code) const {
        if constexpr (in_array) {
            INTERNAL_ERROR("Variable length strings in arrays are not supported");
        } else {
            const lexer::SIZE size_size = string_type->size_size;
            const lexer::SIZE stored_size_size = string_type->stored_size_size;
            const std::string_view size_type_str = get_size_type_str(size_size);
            const Buffer::View<uint64_t> size_chain = offsets_accessor.next_var_offset();

            const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

            const uint16_t size_leaf_idx = (*current_size_leaf_idx)++;
            // console.debug("STRING size_leaf_idx: ", size_leaf_idx);

            auto unique_name = get_unique_name(additional_args);

            auto&& string_data_method = std::move(code)
                ._struct(unique_name)
                .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end()
                .method("const char*", "c_str");

            if (size_chain.empty()) {
                string_data_method = std::move(string_data_method)
                    .line("return reinterpret_cast<const char*>(base + ", offsets_accessor.var_leafs_start, ");");
            } else {
                string_data_method = std::move(string_data_method)
                    .line("return reinterpret_cast<const char*>(base + ", offsets_accessor.var_leafs_start, SizeChainCodeGenerator{offsets_accessor.var_offset_buffer, size_chain}, ");");
            }

            auto&& string_size_method = std::move(string_data_method)
                .end()
                .method(size_type_str, "size");
            
            if constexpr (is_dynamic_variant_element<Args>) {
                const uint64_t offset = offsets_accessor.next_fixed_offset();
                string_size_method = std::move(string_size_method)
                    .line("return ", string_type->min_length, " + *reinterpret_cast<", get_size_type_str(stored_size_size), "*>(base + ", offset, ");");
            } else {
                level_size_leafs[size_leaf_idx] = {
                    string_type->min_length,
                    offsets_accessor.next_map_idx(),
                    size_size,
                    stored_size_size
                };
                string_size_method = std::move(string_size_method)
                    .line("return size", size_leaf_idx, "(base);");
            }

            return gen_field_access_method_no_array(
                std::move(string_size_method)
                    .end()
                    .method(size_type_str, "length")
                    .line("return size() - 1;")
                    .end()
                    ._private()
                    .field("size_t", "base")
                    .end(),
                additional_args,
                array_ctor_strs.ctor_used,
                unique_name
            );
        }
    }

    [[nodiscard]] result_t on_fixed_array (const lexer::ArrayType* const fixed_array_type, codegen::UnknownStructBase&& code) const {
        const uint32_t length = fixed_array_type->length;
        const std::string_view size_type_str = get_size_type_str(fixed_array_type->size_size);

        const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        uint16_t depth;
        if constexpr (std::is_same_v<Args, GenFixedArrayLeafArgs>) {
            // only fixed arrays can be nested
            depth = additional_args.depth + 1;
        } else {
            depth = 0;
        }

        auto unique_name = get_unique_name(additional_args, [&depth]() { return codegen::StringParts{"Array_"_sl, depth}; });

        /* uint32_t* new_array_lengths = nullptr;
        if (array_depth > 0) {
            new_array_lengths = ALLOCA(uint32_t, array_depth);
            const uint8_t last_i = array_depth - 1;
            for (uint8_t i = 0; i < last_i; i++) {
                new_array_lengths[i] = array_lengths.data[i];
            }
            new_array_lengths[last_i] = length;
        } */

        result_t result = fixed_array_type->inner_type()->visit(
            TypeVisitor<
                next_type_t,
                is_fixed,
                true,
                GenFixedArrayLeafArgs,
                decltype(estd::conditionally<is_dynamic_variant_element<Args>>(unique_name, base_name))
            >{
                ast_buffer,
                estd::conditionally<is_dynamic_variant_element<Args>>(unique_name, base_name),
                offsets_accessor,
                level_size_leafs,
                current_size_leaf_idx,
                GenFixedArrayLeafArgs{depth},
                gsl::narrow_cast<uint8_t>(array_depth + 1),
                pack_sizes
            },
            std::move(code)
                ._struct(unique_name)
                .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end()
                .template as<codegen::UnknownStructBase>()
        );

        auto&& array_struct = std::move(result.value).template as<codegen::NestedStruct<codegen::UnknownStructBase>>()
            .method(codegen::Attributes{"constexpr"}, size_type_str, "length")
                .line("return ", length, ";")
            .end()
            ._private()
            .field("size_t", "base");

        for (uint8_t i = 0; i < array_depth; i++) {
            array_struct = std::move(array_struct)
            .field("uint32_t", codegen::StringParts{"idx_", i});
        }
        if constexpr (is_dynamic_variant_element<Args>) {
            BSSERT(level_size_leafs.size() == 0, "Unexpected state");
        }

        code = std::move(array_struct)
        .end();
        
        if constexpr (is_array_element<Args>) {
            code = std::move(code)
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


    [[nodiscard]] result_t on_array (const lexer::ArrayType* const array_type, codegen::UnknownStructBase&& code) const {
        if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
            const lexer::SIZE size_size = array_type->size_size;
            const lexer::SIZE stored_size_size = array_type->stored_size_size;
            const std::string_view size_type_str = get_size_type_str(size_size);

            const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(0);

            auto unique_name = get_unique_name(additional_args);

            result_t result = array_type->inner_type()->visit(
                TypeVisitor<
                    next_type_t,
                    false,
                    true,
                    GenArrayLeafArgs,
                    decltype(estd::conditionally<is_dynamic_variant_element<Args>>(unique_name, base_name))
                >{
                    ast_buffer,
                    estd::conditionally<is_dynamic_variant_element<Args>>(unique_name, base_name),
                    offsets_accessor,
                    level_size_leafs,
                    current_size_leaf_idx,
                    GenArrayLeafArgs{},
                    1,
                    pack_sizes
                },
                std::move(code)
                ._struct(unique_name)
                .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end()
                .template as<codegen::UnknownStructBase>()
            );

            const uint16_t size_leaf_idx = (*current_size_leaf_idx)++;
            // console.debug("ARRAY size_leaf_idx:", size_leaf_idx);
            level_size_leafs[size_leaf_idx] = {
                array_type->length,
                offsets_accessor.next_map_idx(),
                size_size,
                stored_size_size
            };

            auto&& array_struct = std::move(result.value).template as<codegen::NestedStruct<codegen::UnknownStructBase>>()
                .method(size_type_str, "length")
                    .line("return size", size_leaf_idx, "(base);")
                .end()
                ._private()
                .field("size_t", "base");
            if constexpr (is_dynamic_variant_element<Args>) {
                array_struct = add_size_leafs(level_size_leafs, offsets_accessor.fixed_offsets, std::move(array_struct));
            }

            code = gen_field_access_method_no_array(
                std::move(array_struct)
                    .end(), additional_args,
                    array_ctor_strs.ctor_used,
                    unique_name
            );

            return {
                std::move(result.next_type),
                std::move(code)
            };
        }
    }

    [[nodiscard]] codegen::UnknownStructBase on_fixed_variant (const lexer::FixedVariantType* const fixed_variant_type, codegen::UnknownStructBase&& code) const {
        const uint16_t variant_count = fixed_variant_type->variant_count;

        auto unique_name = get_unique_name<"Variant">(additional_args);

        const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);
        
        auto&& variant_struct = std::move(code)
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        if (variant_count <= UINT8_MAX) {
            variant_struct = gen_value_leaf<is_fixed, false, in_array, "uint8_t", lexer::SIZE::SIZE_1>(std::move(variant_struct), offsets_accessor, "id"_sl, pack_info_idx, array_depth);
        } else {
            variant_struct = gen_value_leaf<is_fixed, false, in_array, "uint16_t", lexer::SIZE::SIZE_2>(std::move(variant_struct), offsets_accessor, "id"_sl, pack_info_idx, array_depth);
        }              

        const lexer::Type* type = fixed_variant_type->first_variant();
        // uint64_t max_offset = 0;

        uint16_t variant_depth;
        if constexpr (is_variant_element<Args>) {
            variant_depth = additional_args.variant_depth + 1;
        } else {
            variant_depth = 0;
        }
        
        for (uint16_t i = 0; i < variant_count; i++) {
            // GenFixedVariantLeafArgs<in_array> gen_variant_leaf_args;
            // if constexpr (in_array) {
            //     gen_variant_leaf_args = GenFixedVariantLeafArgs<true>{{i, variant_depth}, max_size, idx_calc_str};
            // } else {
            //     gen_variant_leaf_args = GenFixedVariantLeafArgs<false>{{i, variant_depth}};
            // }
            
            lexer::Type::VisitResult<lexer::Type, codegen::UnknownStructBase> result = type->visit(TypeVisitor<
                lexer::Type,
                is_fixed,
                in_array,
                GenFixedVariantLeafArgs,
                decltype(estd::conditionally<is_dynamic_variant_element<Args>>(unique_name, base_name))
            >{
                ast_buffer,
                estd::conditionally<is_dynamic_variant_element<Args>>(unique_name, base_name),
                offsets_accessor,
                std::span<SizeLeaf>{},
                current_size_leaf_idx,
                GenFixedVariantLeafArgs{
                    {i, variant_depth}
                },
                array_depth,
                lexer::LeafSizes::zero()
            }, std::move(variant_struct).template as<codegen::UnknownStructBase>());

            type = result.next_type;
            variant_struct = std::move(result.value).template as<codegen::NestedStruct<codegen::UnknownStructBase>>();
        }
        
        variant_struct = std::move(variant_struct)
        ._private()
        .field("size_t", "base");

        for (uint8_t i = 0; i < array_depth; i++) {
            variant_struct = std::move(variant_struct)
            .field("uint32_t", codegen::StringParts{"idx_", i});
        }
        if constexpr (is_dynamic_variant_element<Args>) {
            variant_struct = add_size_leafs(level_size_leafs, offsets_accessor.fixed_offsets, std::move(variant_struct));
        }

        code = std::move(variant_struct)
        .end();

        if constexpr (is_array_element<Args>) {
            return std::move(code)
            .method("Variant", "get", codegen::Args{"uint32_t idx"})
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            return gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
        }
    }

    [[nodiscard]] codegen::UnknownStructBase on_packed_variant (const lexer::PackedVariantType* const /*unused*/, codegen::UnknownStructBase&&  /*unused*/) const {
        INTERNAL_ERROR("Packed variant not supported");
    }

    [[nodiscard]] codegen::UnknownStructBase on_dynamic_variant (const lexer::DynamicVariantType* const dynamic_variant_type, codegen::UnknownStructBase&& code) const {
        if constexpr (in_array) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
            const uint16_t variant_count = dynamic_variant_type->variant_count;

            auto unique_name = get_unique_name<"DynamicVariant">(additional_args);

            const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);
            
            auto&& variant_struct = std::move(code)
            ._struct(unique_name)
                .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();
            
            if (variant_count <= UINT8_MAX) {
                variant_struct = gen_value_leaf<is_fixed, false, in_array, "uint8_t", lexer::SIZE::SIZE_1>(std::move(variant_struct), offsets_accessor, "id"_sl, pack_info_idx, array_depth);
            } else {
                variant_struct = gen_value_leaf<is_fixed, false, in_array, "uint16_t", lexer::SIZE::SIZE_2>(std::move(variant_struct), offsets_accessor, "id"_sl, pack_info_idx, array_depth);
            }

            const uint16_t size_leaf_idx = (*current_size_leaf_idx)++;
            // console.debug("ARRAY size_leaf_idx: ", size_leaf_idx);
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
                char* buf = ALLOCA(char, buf_size);
                char* end = generator.write(buf);
                auto d = end - buf;
                BSSERT(d >= 0 && buf_size == gsl::narrow_cast<size_t>(d), " ", buf_size, " == ", std::bit_cast<uintptr_t>(end), " - ", std::bit_cast<uintptr_t>(buf));
                offset = {buf, buf_size};
            }

            const lexer::Type* type = dynamic_variant_type->first_variant();

            uint16_t variant_depth;
            if constexpr (is_variant_element<Args>) {
                variant_depth = additional_args.variant_depth + 1;
            } else {
                variant_depth = 0;
            }
            uint16_t max_level_size_leafs = 0;
            for (uint16_t i = 0; i < variant_count; i++) {
                max_level_size_leafs = std::max(
                    dynamic_variant_type->type_metas()[i].level_size_leafs,
                    max_level_size_leafs
                );
            }
            ALLOCA_SAFE(sublevel_size_leafs_buffer, SizeLeaf, max_level_size_leafs);
            
            for (uint16_t i = 0; i < variant_count; i++) {
                const auto& type_meta = dynamic_variant_type->type_metas()[i];
                const auto level_size_leafs_count = type_meta.level_size_leafs;

                // GenFixedVariantLeafArgs<in_array> gen_variant_leaf_args;
                // if constexpr (in_array) {
                //     gen_variant_leaf_args = GenFixedVariantLeafArgs<true>{{i, variant_depth}, max_size, idx_calc_str};
                // } else {
                //     gen_variant_leaf_args = GenFixedVariantLeafArgs<false>{{i, variant_depth}};
                // }
                uint16_t current_size_leaf_idx = 0;
                result_t result = type->visit(TypeVisitor<
                    next_type_t,
                    true,
                    in_array,
                    GenDynamicVariantLeafArgs,
                    decltype(estd::conditionally<is_dynamic_variant_element<Args>>(unique_name, base_name))
                >{
                    ast_buffer,
                    estd::conditionally<is_dynamic_variant_element<Args>>(unique_name, base_name),
                    offsets_accessor,
                    std::span<SizeLeaf>{
                        sublevel_size_leafs_buffer,
                        sublevel_size_leafs_buffer + level_size_leafs_count
                    },
                    &current_size_leaf_idx,
                    GenDynamicVariantLeafArgs{
                        offset,
                        {i, variant_depth}
                    },
                    array_depth,
                    pack_sizes
                }, std::move(variant_struct).template as<codegen::UnknownStructBase>());
                type = (lexer::Type*)result.next_type;
                variant_struct = std::move(result.value).template as<codegen::NestedStruct<codegen::UnknownStructBase>>();
            }

            variant_struct = std::move(variant_struct)
                ._private()
                .field("size_t", "base");

            for (uint8_t i = 0; i < array_depth; i++) {
                variant_struct = std::move(variant_struct)
                .field("uint32_t", codegen::StringParts{"idx_", i});
            }
            if constexpr (is_dynamic_variant_element<Args>) {
                // console.debug("Adding size leafs, variant_depth: ", additional_args.variant_depth);
                variant_struct = add_size_leafs(level_size_leafs, offsets_accessor.fixed_offsets, std::move(variant_struct));
            }

            static_assert(!is_array_element<Args>, "Dynamic variant cant be array element");
            static_assert(!in_array, "Dynamic variant cant be in array");

            return gen_field_access_method_no_array(
                std::move(variant_struct)
                    .end(),
                additional_args,
                array_ctor_strs.ctor_used,
                unique_name
            );
        }
    }

    [[nodiscard]] codegen::UnknownStructBase on_identifier (const lexer::IdentifiedType* const identified_type, codegen::UnknownStructBase&& code) const {
        const auto* const identifier = ast_buffer.get(identified_type->identifier_idx);
        if (identifier->keyword != lexer::KEYWORDS::STRUCT) {
            INTERNAL_ERROR("not implemented");
        }
        const auto* const struct_type = identifier->data()->as_struct();

        const ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto unique_name = get_unique_name(additional_args, [&struct_type]() { return struct_type->name; });
        
        auto&& struct_code = std::move(code)
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        struct_type->visit([&](auto field_data) {
            uint16_t struct_depth;
            if constexpr (std::is_same_v<Args, GenStructLeafArgs>) {
                struct_depth = additional_args.depth + 1;
            } else {
                struct_depth = 0;
            }

            lexer::Type::VisitResult<lexer::StructField, codegen::UnknownStructBase> result = field_data->type()->visit(TypeVisitor<
                lexer::StructField,
                is_fixed,
                in_array,
                GenStructLeafArgs,
                decltype(estd::conditionally<is_dynamic_variant_element<Args>>(unique_name, base_name))
            >{
                ast_buffer,
                estd::conditionally<is_dynamic_variant_element<Args>>(unique_name, base_name),
                offsets_accessor,
                level_size_leafs,
                current_size_leaf_idx,
                GenStructLeafArgs{field_data->name, struct_depth},
                array_depth,
                pack_sizes
            }, std::move(struct_code).template as<codegen::UnknownStructBase>());

            struct_code = std::move(result.value).template as<codegen::NestedStruct<codegen::UnknownStructBase>>();
            return result.next_type;
        });

        struct_code = std::move(struct_code)
            ._private()
            .field("size_t", "base");
        
        for (uint8_t i = 0; i < array_depth; i++) {
            struct_code = std::move(struct_code)
            .field("uint32_t", codegen::StringParts{"idx_", i});
        }

        if constexpr (is_dynamic_variant_element<Args>) {
            struct_code = add_size_leafs(level_size_leafs, offsets_accessor.fixed_offsets, std::move(struct_code));
        }

        code = std::move(struct_code)
        .end();

        if constexpr (is_array_element<Args>) {
            return std::move(code)
            .method(unique_name, "get", codegen::Args{"uint32_t idx"})
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            return gen_field_access_method_no_array(std::move(code), additional_args, array_ctor_strs.ctor_used, unique_name);
        }
    }
};

void generate (
    const lexer::StructDefinition* const target_struct,
    const ReadOnlyBuffer& ast_buffer,
    const int output_fd
) {
    Buffer code_buffer = BUFFER_INIT_STACK(1 << 14);
    const lexer::LeafCounts level_fixed_leafs = target_struct->level_fixed_leafs;
    const lexer::LeafCounts::Counts& var_leaf_counts = target_struct->var_leaf_counts.counts();
    const uint16_t level_fixed_variants = target_struct->level_fixed_variants;
    const uint16_t level_fixed_arrays = target_struct->level_fixed_arrays;
    const uint16_t level_fixed_leafs_total = level_fixed_leafs.total();
    const uint16_t total_var_leafs = var_leaf_counts.total();
    const uint16_t sublevel_fixed_leafs = target_struct->sublevel_fixed_leafs;
    const uint16_t total_variant_var_leafs = target_struct->total_variant_var_leafs;
    const uint16_t total_leafs = level_fixed_leafs_total + total_var_leafs + sublevel_fixed_leafs  + total_variant_var_leafs;
    const uint16_t level_size_leafs_count = target_struct->level_size_leafs;
    console.debug("level_fixed_leafs ", level_fixed_leafs.counts());
    console.debug("var_leaf_counts ", var_leaf_counts);
    console.debug("level_fixed_variants: ", level_fixed_variants);
    console.debug("level_variant_fields: ", target_struct->level_variant_fields, " vs ", level_fixed_variants);
    console.debug("sublevel_fixed_leafs: ", sublevel_fixed_leafs);
    console.debug("total_variant_var_leafs: ", total_variant_var_leafs);
    console.debug("total_leafs: ", total_leafs);
    console.debug("level_size_leafs: ", level_size_leafs_count);

    const std::string_view struct_name = target_struct->name;
    // Any struct and therfore target requires at least one member has
    ALLOCA_UNSAFE_SPAN(fixed_offsets, FixedOffset, level_fixed_leafs_total + sublevel_fixed_leafs);
    std::ranges::uninitialized_fill(fixed_offsets, FixedOffset::empty());
    ALLOCA_SAFE_SPAN(var_offsets, Buffer::View<uint64_t>, total_var_leafs + total_variant_var_leafs);
    std::ranges::uninitialized_fill(var_offsets, Buffer::View<uint64_t>{Buffer::Index<uint64_t>{static_cast<Buffer::index_t>(-1)}, 0});
    // total_leafs has the fixed leaf count in its sum which is garunteed to be at least 1
    ALLOCA_UNSAFE_SPAN(idx_map, uint16_t, total_leafs);
    std::ranges::uninitialized_fill(idx_map, static_cast<uint16_t>(-1));

    ALLOCA_SAFE_SPAN(pack_infos, ArrayPackInfo, target_struct->pack_count);
    std::ranges::uninitialized_fill(pack_infos, ArrayPackInfo{0, static_cast<uint16_t>(-1)});

    auto generate_offsets_result = generate_offsets::generate(
        target_struct,
        ast_buffer,
        fixed_offsets,
        var_offsets,
        idx_map,
        pack_infos,
        BUFFER_INIT_STACK(sizeof(uint64_t) * 512),
        level_fixed_leafs,
        var_leaf_counts,
        total_var_leafs,
        level_fixed_variants,
        level_fixed_arrays,
        level_size_leafs_count
    );

    uint16_t current_map_idx = 0;
    uint16_t current_variant_field_idx = 0;
    OffsetsAccessor offsets_accessor {
        fixed_offsets,
        var_offsets,
        idx_map,
        pack_infos,
        ReadOnlyBuffer{generate_offsets_result.var_offset_buffer},
        generate_offsets_result.var_leafs_start,
        &current_map_idx,
        &current_variant_field_idx
    };

    ALLOCA_SAFE_SPAN(level_size_leafs, SizeLeaf, level_size_leafs_count);
    uint16_t current_size_leaf_idx = 0;

    const auto codegen_start_ts = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; ; i++) {
        auto&& code = codegen::create_code(std::move(code_buffer))
        .line("#include \"lib/lib.hpp\"")
        .line("");

        auto&& struct_code = std::move(code)
        ._struct(struct_name)
            .ctor("size_t base", "base(base)").end();

        target_struct->visit([&](const lexer::StructField::Data* const field_data) {
            auto name = field_data->name;
            auto result = field_data->type()->visit(TypeVisitor<
                lexer::StructField,
                true,
                false,
                GenStructLeafArgs,
                std::string_view
            >{
                ast_buffer,
                struct_name,
                offsets_accessor,
                level_size_leafs,
                &current_size_leaf_idx,
                GenStructLeafArgs{name, 0},
                0,
                lexer::LeafSizes::zero()
            }, std::move(struct_code).template as<codegen::UnknownStructBase>());

            struct_code = std::move(result.value).template as<std::remove_reference_t<decltype(struct_code)>>();
            return result.next_type;
        });

        struct_code = std::move(struct_code)
            ._private()
            .field("size_t", "base");

        struct_code = add_size_leafs(level_size_leafs, fixed_offsets, std::move(struct_code));

        auto&& code_done = std::move(struct_code)
        .end()
        .end();

        const bool is_last = i == 100000;

        if (is_last) {
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

        code_buffer = std::move(code_done.buffer);
        code_buffer.clear();
        current_map_idx = 0;
        current_variant_field_idx = 0;

        if (is_last) break;
    }

    const auto codegen_end_ts = std::chrono::high_resolution_clock::now();

    console.info("Codegen took ", std::chrono::duration_cast<std::chrono::milliseconds>(codegen_end_ts - codegen_start_ts).count(), " ms");
}


}