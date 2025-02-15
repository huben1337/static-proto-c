#pragma once
#include <cstdint>
#include <io.h>
#include <cstdio>
#include <string>
#include <array>
#include <memory>
#include <algorithm>
#include "base.cpp"
#include "codegen.cpp"
#include "lexer_types.cpp"
#include "string_helpers.cpp"
#include "string_literal.cpp"
#include "code_gen_stuff.cpp"

namespace decode_code {


struct SizeLeaf {
    uint32_t leaf_idx;
    uint32_t min_size;
    lexer::SIZE size_size;
    lexer::SIZE stored_size_size;
};

struct VariantField {
    uint64_t size;
    uint32_t start;
    uint32_t end;
};

enum MODE {
    DEFAULT         = 0b0000,
    FIXED_ARRAY     = 0b0001,
    ARRAY           = 0b0010,
    VARIANT         = 0b0100,
};
template <MODE target, MODE mode>
constexpr bool is_mode () {
    if constexpr (target == MODE::DEFAULT) {
        return mode == MODE::DEFAULT;
    } else {
        return mode & target;
    }
}
template <MODE target>
constexpr bool is_mode (MODE mode) {
    if constexpr (target == MODE::DEFAULT) {
        return mode == MODE::DEFAULT;
    } else {
        return mode & target;
    }
}

struct CodeChunks {
    static INLINE CodeChunks* create (
        uint8_t* mem,
        uint16_t total_leafs,
        uint16_t variant_base_idx
    ) {
        CodeChunks* self = reinterpret_cast<CodeChunks*>(mem);
        
        self->total_leafs = total_leafs;
        self->current_map_idx = 0;
        self->current_variant_base_idx = variant_base_idx;

        return self;
    }

    static INLINE size_t mem_size (uint16_t total_leafs) {
        return (sizeof(CodeChunks) + 2) + total_leafs * (sizeof(uint64_t) + sizeof(Buffer::Index<char>) + sizeof(uint16_t));
    }

    uint16_t total_leafs;
    uint16_t current_map_idx;
    uint16_t current_variant_base_idx;

    INLINE auto sizes () {
        return reinterpret_cast<uint64_t*>(reinterpret_cast<size_t>(this) + (sizeof(CodeChunks) + 2));
    }
    INLINE auto chunk_starts () {
        return reinterpret_cast<Buffer::Index<char>*>(reinterpret_cast<size_t>(sizes()) + total_leafs * sizeof(uint64_t));
    }
    INLINE auto chunk_map () {
        return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(chunk_starts()) + total_leafs * sizeof(Buffer::Index<char>));
    }
};


template <bool in_variant>
struct Leafs {
    Leafs () = default;
    Leafs (
        CodeChunks* cc,
        uint8_t* data,
        lexer::LeafCounts::Counts fixed_leaf_counts,
        lexer::LeafCounts::Counts var_leaf_counts,
        lexer::LeafCounts::Counts variant_field_counts,
        uint16_t total_fixed_leafs,
        uint16_t total_var_leafs,
        uint16_t total_size_leafs,
        uint16_t level_variant_fields,
        uint16_t current_variant_idx_base
    )
    :   cc(cc),
        data(data),
        fixed_leaf_counts(fixed_leaf_counts),
        var_leaf_counts(var_leaf_counts),
        variant_field_counts(variant_field_counts),
        total_fixed_leafs(total_fixed_leafs),
        total_var_leafs(total_var_leafs),
        total_size_leafs(total_size_leafs),
        level_variant_fields(level_variant_fields),
        current_variant_idx_base(current_variant_idx_base)
    {   
        if constexpr (in_variant) {
            *current_variant_idx() = current_variant_idx_base;
        }
        *current_size_leaf_idx() = 0;
        *fixed_leaf_positions() = {0, 0, 0, 0};
        *var_leaf_positions() = {0, 0, 0, 0};
        *variant_field_positions() = {0, 0, 0, 0};
    }

    CodeChunks* const cc;
    uint8_t* const data;
    const lexer::LeafCounts::Counts fixed_leaf_counts;
    const lexer::LeafCounts::Counts var_leaf_counts;
    const lexer::LeafCounts::Counts variant_field_counts;
    const uint16_t total_fixed_leafs;
    const uint16_t total_var_leafs;
    const uint16_t total_size_leafs;
    const uint16_t level_variant_fields;
    const uint16_t current_variant_idx_base;

    static INLINE size_t mem_size (uint16_t total_var_leafs, uint16_t total_size_leafs, uint16_t level_variant_fields) {
        return sizeof(lexer::LeafCounts::Counts) * 3 + level_variant_fields * sizeof(VariantField) + total_size_leafs * sizeof(SizeLeaf) + total_var_leafs * sizeof(uint16_t) + sizeof(uint16_t) * 2;
    }

    template <MODE mode>
    INLINE auto reserve_next (lexer::SIZE size, uint64_t length) {
        switch (size) {
        case lexer::SIZE_1:
            return reserve_size8<mode>(length);
        case lexer::SIZE_2:
            return reserve_size16<mode>(length);
        case lexer::SIZE_4:
            return reserve_size32<mode>(length);
        case lexer::SIZE_8:
            return reserve_size64<mode>(length);
        default:
            printf("[reserve_next] invalid size\n");
            exit(1);
        }
    }
    template <MODE mode, lexer::SIZE size>
    INLINE auto next (uint16_t idx, uint64_t length) {
        if constexpr (is_mode<MODE::ARRAY, mode>()) {
            size_leafe_idxs()[idx] = *current_size_leaf_idx();
            idx += total_fixed_leafs;
        }
        uint16_t cc_idx = current_variant_idx_base + idx;
        if constexpr (in_variant) {
            current_variant_idx()[0]++;
        }
        cc->sizes()[cc_idx] = length * size;
        uint16_t cc_map_idx = cc->current_map_idx++;
        printf("cc_map_idx: %d, cc_idx: %d, size: %d, length: %d, variant\n", cc_map_idx, cc_idx, size * length, length);
        cc->chunk_map()[cc_map_idx] = cc_idx;
        return cc->chunk_starts() + cc_idx;
    }
    template <MODE mode>
    INLINE std::pair<lexer::LeafCounts::Counts, lexer::LeafCounts::Counts*> get_counts_and_positions () {
        if constexpr (is_mode<MODE::DEFAULT, mode>() || is_mode<MODE::FIXED_ARRAY, mode>() || is_mode<MODE::VARIANT, mode>()) {
            return {fixed_leaf_counts, fixed_leaf_positions()};
        } else if constexpr (is_mode<MODE::ARRAY, mode>()) {
            return {var_leaf_counts, var_leaf_positions()};
        }
    }
    template <MODE mode>
    INLINE uint16_t reserve_idx (lexer::SIZE size) {
        auto [counts, positions] = get_counts_and_positions<mode>();
        switch (size) {
        case lexer::SIZE_1:
            if (positions->size8 == counts.size8) {
                printf("[CodeChunks] position overflow %s\n", is_mode<MODE::ARRAY, mode>() ? "array" : "default");
                printf("counts.size8: %d\n", counts.size8);
            }
            return counts.size64 + counts.size32 + counts.size16 + positions->size8++;
        case lexer::SIZE_2:
            if (positions->size16 == counts.size16) {
                printf("[CodeChunks] position overflow %s\n", is_mode<MODE::ARRAY, mode>() ? "array" : "default");
            }
            return counts.size64 + counts.size32 + positions->size16++;
        case lexer::SIZE_4:
            if (positions->size32 == counts.size32) {
                printf("[CodeChunks] position overflow %s\n", is_mode<MODE::ARRAY, mode>() ? "array" : "default");
            }
            return counts.size64 + positions->size32++;
        case lexer::SIZE_8:
            if (positions->size64 == counts.size64) {
                printf("[CodeChunks] position overflow %s\n", is_mode<MODE::ARRAY, mode>() ? "array" : "default");
            }
            return positions->size64++;
        default:
            printf("[reserve_idx, fixed] invalid size\n");
            exit(1);
        }
    }
    template <MODE mode>
    INLINE auto reserve_size8 (uint64_t length) {
        auto [counts, positions] = get_counts_and_positions<mode>();
        uint16_t idx = counts.size64 + counts.size32 + counts.size16 + positions->size8++;
        return next<mode, lexer::SIZE_1>(idx, length);
    }
    template <MODE mode>
    INLINE auto reserve_size16 (uint64_t length) {
        auto [counts, positions] = get_counts_and_positions<mode>();
        uint16_t idx = counts.size64 + counts.size32 + positions->size16++;
        return next<mode, lexer::SIZE_2>(idx, length);
    }
    template <MODE mode>
    INLINE auto reserve_size32 (uint64_t length) {
        auto [counts, positions] = get_counts_and_positions<mode>();
        uint16_t idx = counts.size64 + positions->size32++;
        return next<mode, lexer::SIZE_4>(idx, length);
    }
    template <MODE mode>
    INLINE auto reserve_size64 (uint64_t length) {
        auto [counts, positions] = get_counts_and_positions<mode>();
        uint16_t idx = positions->size64++;
        return next<mode, lexer::SIZE_8>(idx, length);
    }
    INLINE auto reserve_variant_field (lexer::SIZE size) {
        uint16_t idx;
        switch (size) {
        case lexer::SIZE_1:
            idx = variant_field_counts.size64 + variant_field_counts.size32 + variant_field_counts.size16 + variant_field_positions()->size8++;
            break;
        case lexer::SIZE_2:
            idx = variant_field_counts.size64 + variant_field_counts.size32 + variant_field_positions()->size16++;
            break;
        case lexer::SIZE_4:
            idx = variant_field_counts.size64 + variant_field_positions()->size32++;
            break;
        case lexer::SIZE_8:
            idx = variant_field_positions()->size64++;
            break;
        }
        return variant_fields() + idx;
    }
    INLINE lexer::LeafCounts::Counts* fixed_leaf_positions () {
        return reinterpret_cast<lexer::LeafCounts::Counts*>(reinterpret_cast<size_t>(data));
    }
    INLINE lexer::LeafCounts::Counts* var_leaf_positions () {
        return reinterpret_cast<lexer::LeafCounts::Counts*>(reinterpret_cast<size_t>(fixed_leaf_positions()) + sizeof(lexer::LeafCounts::Counts));
    }
    INLINE lexer::LeafCounts::Counts* variant_field_positions () {
        return reinterpret_cast<lexer::LeafCounts::Counts*>(reinterpret_cast<size_t>(var_leaf_positions()) + sizeof(lexer::LeafCounts::Counts));
    }
    INLINE uint16_t* current_size_leaf_idx () {
        return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(variant_field_positions()) + sizeof(lexer::LeafCounts::Counts));
    }
    INLINE uint16_t* current_variant_idx () {
        if constexpr (in_variant) {
            return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(current_size_leaf_idx()) + sizeof(uint16_t));
        } else {
            return nullptr;
        }
        
    }
    INLINE VariantField* variant_fields () {
        if constexpr (in_variant) {
            return reinterpret_cast<VariantField*>(reinterpret_cast<size_t>(current_variant_idx()) + sizeof(uint16_t) + 2);
        } else {
            return reinterpret_cast<VariantField*>(reinterpret_cast<size_t>(current_size_leaf_idx()) + sizeof(uint16_t) + 3);
        }
    }
    INLINE SizeLeaf* size_leafs () {
        return reinterpret_cast<SizeLeaf*>(reinterpret_cast<size_t>(variant_fields()) + level_variant_fields * sizeof(VariantField));
    }
    INLINE auto size_leafe_idxs () {
        return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(size_leafs()) + total_size_leafs * sizeof(SizeLeaf));
    }
    INLINE uint16_t next_size_leaf_idx () {
        return current_size_leaf_idx()[0]++;
    }
};

INLINE std::string get_size_type_str (lexer::SIZE size) {
    switch (size)
    {
    case lexer::SIZE_1:
        return "uint8_t";
    case lexer::SIZE_2:
        return "uint16_t";
    case lexer::SIZE_4:
        return "uint32_t";
    case lexer::SIZE_8:
        return "uint64_t";
    default:
        printf("[get_size_type_str] invalid size\n");
        exit(1);
    }
}

INLINE std::string get_size_type_str (uint32_t length) {
    if (length <= UINT8_MAX) {
        return "uint8_t";
    } else if (length <= UINT16_MAX) {
        return "uint16_t";
    } else /* if (length <= UINT32_MAX) */ {
        return "uint32_t";
    }
}

INLINE uint64_t get_size_size (uint32_t length) {
    if (length <= UINT8_MAX) {
        return 1;
    } else if (length <= UINT16_MAX) {
        return 2;
    } else /* if (length <= UINT32_MAX) */ {
        return 4;
    }
}

struct ArrayCtorStrs {
    std::string_view ctor_args;
    std::string_view ctor_inits;
    std::string_view ctor_used;
    std::string_view el_ctor_used;
};
/* TODO: Create a lookup table for the first 65 elements. (65 = 64 + 1. Since fixed arrays can only be of length 2 or more 64 nested is the max and 1 more for a outer dynamic array. If bools are packed perfectly add 3 more)*/
ArrayCtorStrs make_array_ctor_strs (uint8_t array_depth) {
    if (array_depth > array_ctor_strs_count) {
        printf("[make_array_ctor_strs] array depth too large\n");
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


template <size_t N, typename T>
INLINE constexpr T next_multiple (T value) {
    if constexpr (N == 1) {
        return value;
    } else if constexpr ((is_power_of_two(N))) {
        constexpr T mask = N - 1;
        return (value + mask) & ~mask;
    } else {
        return (value + N - 1) / N * N;
    }
}

template <typename T>
INLINE constexpr T next_multiple (T value, lexer::SIZE base) {
    T mask = static_cast<T>(base) - 1;
    return (value + mask) & ~mask;
}

INLINE uint64_t get_idx_size_multiplier (uint32_t* array_lengths, uint8_t array_depth, uint8_t i) {
    uint64_t size = 1;
    uint32_t* array_lengths_end = array_lengths + array_depth - 1 - i;
    for (uint32_t* array_length = array_lengths; array_length < array_lengths_end; array_length++) {
        size *= *array_length;
    }
    return size;
}

template <bool no_multiply, bool last_is_direct = false>
INLINE std::string make_idx_calc (uint32_t* array_lengths, uint8_t array_depth) {
    

    if (array_depth == 1) {
        if constexpr (last_is_direct) {
            return "idx";
        } else {
            return "idx_0";
        }
    } else {
        auto add_idx = [array_lengths, array_depth]<bool is_last>(uint32_t i)->std::string {
            if constexpr (is_last) {
                if constexpr (last_is_direct) {
                    return " + idx";
                } else {
                    return " + idx_" + std::to_string(i);
                }
            } else {
                return " + idx_" + std::to_string(i) + " * " + std::to_string(get_idx_size_multiplier(array_lengths, array_depth, i));
            }
        };

        std::string idx_calc;

        if  constexpr (no_multiply) {
            idx_calc = "idx_0 * " + std::to_string(get_idx_size_multiplier(array_lengths, array_depth, 0));
        } else {
            idx_calc = "(idx_0 * " + std::to_string(get_idx_size_multiplier(array_lengths, array_depth, 0));
        }

        for (uint32_t i = 1; i < array_depth - 1; i++) {
            idx_calc += add_idx.template operator()<false>(i);
        }

        if constexpr (no_multiply) {
            return idx_calc + add_idx.template operator()<true>(array_depth - 1);
        } else {
            return idx_calc + add_idx.template operator()<true>(array_depth - 1) + ")";
        }
    }
}

template <typename F, typename Ret, typename... Args>
concept CallableWith = requires(F func, Args... args) {
    { func(args...) } -> std::same_as<Ret>;
};

template <Integral T>
struct Range {
    T start;
    T end;
};

template <lexer::SIZE size>
INLINE Range<uint16_t> get_range (lexer::LeafCounts::Counts counts) {
    uint16_t start;
    uint16_t end;
    if constexpr (size == lexer::SIZE::SIZE_8) {
        start = 0;
        end = counts.size64;
    } else if constexpr (size == lexer::SIZE::SIZE_4) {
        start = counts.size64;
        end = start + counts.size32;
    } else if constexpr (size == lexer::SIZE::SIZE_2) {
        start = counts.size64 + counts.size32;
        end = start + counts.size16;
    } else {
        start = counts.size64 + counts.size32 + counts.size16;
        end = start + counts.size8;
    }
    return {start, end};
}

template <lexer::SIZE size>
INLINE uint16_t get_count (lexer::LeafCounts::Counts counts) {
    if constexpr (size == lexer::SIZE::SIZE_8) {
        return counts.size64;
    } else if constexpr (size == lexer::SIZE::SIZE_4) {
        return counts.size32;
    } else if constexpr (size == lexer::SIZE::SIZE_2) {
        return counts.size16;
    } else {
        return counts.size8;
    }
}

template <lexer::SIZE size>
constexpr lexer::SIZE next_smaller_size () {
    if constexpr (size == lexer::SIZE::SIZE_8) {
        return lexer::SIZE::SIZE_4;
    } else if constexpr (size == lexer::SIZE::SIZE_4) {
        return lexer::SIZE::SIZE_2;
    } else if constexpr (size == lexer::SIZE::SIZE_2) {
        return lexer::SIZE::SIZE_1;
    } else {
        return lexer::SIZE::SIZE_0;
    }
}

template <lexer::SIZE size = lexer::SIZE::SIZE_8, bool aligned = true, CallableWith<uint64_t, uint16_t, uint64_t> F_set_offset, CallableWith<uint64_t, uint16_t, uint64_t> F_add_variant_base>
INLINE uint64_t set_sizes (uint64_t offset, lexer::LeafCounts::Counts fixed_leaf_counts, lexer::LeafCounts::Counts variant_field_counts, F_set_offset set_offset, F_add_variant_base add_variant_base) {
    if constexpr (size == lexer::SIZE_0) {
        return offset;
    } else {
        auto [fixed_leafs_start, fixed_leafs_end] = get_range<size>(fixed_leaf_counts);
        auto [variant_field_start, variant_field_end] = get_range<size>(variant_field_counts);
        if constexpr (aligned) {
            for (uint16_t i = fixed_leafs_start; i < fixed_leafs_end; i++) {
                offset = set_offset(i, offset);
            }
            if (get_count<size>(variant_field_counts) > 0) {
                offset = add_variant_base(variant_field_start, offset);
                for (uint16_t i = variant_field_start + 1; i < variant_field_end; i++) {
                    offset = next_multiple<size>(offset);
                    offset = add_variant_base(i, offset);
                }
                return set_sizes<next_smaller_size<size>(), false>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
            } else {
                return set_sizes<next_smaller_size<size>(), true>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
            }
        } else {
            if (get_count<size>(fixed_leaf_counts) > 0) {
                offset = next_multiple<size>(offset);
                offset = set_offset(fixed_leafs_start, offset);
                for (uint16_t i = fixed_leafs_start + 1; i < fixed_leafs_end; i++) {
                    offset = set_offset(i, offset);
                }
                if (get_count<size>(variant_field_counts) > 0) {
                    offset = add_variant_base(variant_field_start, offset);
                    for (uint16_t i = variant_field_start + 1; i < variant_field_end; i++) {
                        offset = next_multiple<size>(offset);
                        offset = add_variant_base(i, offset);
                    }
                    return set_sizes<next_smaller_size<size>(), false>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
                } else {
                    return set_sizes<next_smaller_size<size>(), true>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
                }
            } else {
                for (uint16_t i = variant_field_start; i < variant_field_end; i++) {
                    offset = next_multiple<size>(offset);
                    offset = add_variant_base(i, offset);
                }
                return set_sizes<next_smaller_size<size>(), false>(offset, fixed_leaf_counts, variant_field_counts, set_offset, add_variant_base);
            }
        }
    }
}

template <typename T, typename Derived>
struct GenStructFieldResult {
    T* next;
    Derived code;
};




struct GenFixedArrayLeafArgs {
    uint64_t outer_array_length;
    uint32_t* array_lengths;
    uint16_t depth;
    uint8_t array_depth;
};

struct GenArrayLeafArgs {
};

struct GenVariantLeafArgs {
    uint16_t variant_id;
    uint16_t variant_depth;
};

struct GenStructLeafArgsDefault {
    std::string_view name;
    uint16_t depth;
};

struct GenStructLeafArgsInArray {
    std::string_view name;
    uint64_t outer_array_length;
    uint32_t* array_lengths;
    uint16_t depth;
    uint8_t array_depth;
};

template <MODE mode>
using GenStructLeafArgs = std::conditional_t<is_mode<MODE::FIXED_ARRAY, mode>(), GenStructLeafArgsInArray, GenStructLeafArgsDefault>;

template <MODE mode, MODE pl_mode>
using additional_args_t =
std::conditional_t<pl_mode == MODE::DEFAULT, GenStructLeafArgs<mode>,
std::conditional_t<pl_mode == MODE::FIXED_ARRAY, GenFixedArrayLeafArgs,
std::conditional_t<pl_mode == MODE::ARRAY, GenArrayLeafArgs,
std::conditional_t<pl_mode == MODE::VARIANT, GenVariantLeafArgs,
void>>>>;

template <MODE pl_mode>
constexpr bool is_array_element = pl_mode == MODE::ARRAY || pl_mode == MODE::FIXED_ARRAY;

template <typename F, typename ... Args>
constexpr size_t arg_count( F(*f)(Args ...))
{
   return sizeof...(Args);
}

template <MODE mode, MODE pl_mode, StringLiteral element_name>
constexpr INLINE auto get_unique_name (const additional_args_t<mode, pl_mode>& additional_args) {
    if constexpr (is_array_element<pl_mode>) {
        return element_name;
    } else if constexpr (pl_mode == MODE::VARIANT) {
        return codegen::StringParts{"_"_sl, additional_args.variant_depth, "_"_sl, additional_args.variant_id};
    } else {
        return codegen::StringParts{additional_args.name, "_"_sl, additional_args.depth};
    }
}

template <MODE mode, MODE pl_mode, typename F>
constexpr INLINE auto get_unique_name (const additional_args_t<mode, pl_mode>& additional_args, F on_element) {
    if constexpr (is_array_element<pl_mode>) {
        return on_element();
    } else if constexpr (pl_mode == MODE::VARIANT) {
        return codegen::StringParts{"_"_sl, additional_args.variant_depth, "_"_sl, additional_args.variant_id};
    } else {
        return codegen::StringParts{additional_args.name, "_"_sl, additional_args.depth};
    }
}

template <MODE mode, MODE pl_mode>
requires (!is_array_element<pl_mode>)
constexpr INLINE auto get_unique_name (const additional_args_t<mode, pl_mode>& additional_args) {
    if constexpr (pl_mode == MODE::VARIANT) {
        return codegen::StringParts{"_"_sl, additional_args.variant_depth, "_"_sl, additional_args.variant_id};
    } else {
        return codegen::StringParts{additional_args.name, "_"_sl, additional_args.depth};
    }
}

template <MODE mode, MODE pl_mode>
requires (!is_array_element<pl_mode>)
constexpr INLINE auto get_name (const additional_args_t<mode, pl_mode>& additional_args, auto unique_name) {
    if constexpr (pl_mode == MODE::VARIANT) {
        return  codegen::StringParts{"as_"_sl, additional_args.variant_id};
    } else {
        return unique_name;
    }
}

template <MODE mode, MODE pl_mode>
requires (!is_array_element<pl_mode>)
constexpr INLINE auto get_name (const additional_args_t<mode, pl_mode>& additional_args) {
    if constexpr (pl_mode == MODE::VARIANT) {
        return  codegen::StringParts{"as_"_sl, additional_args.variant_id};
    } else {
        return codegen::StringParts{additional_args.name, "_"_sl, additional_args.depth};
    }
}

template <MODE mode, MODE pl_mode>
GenStructFieldResult<lexer::StructField, codegen::__UnknownStruct> gen_leaf (
    lexer::Type* field_type,
    Buffer &buffer,
    codegen::__UnknownStruct code,
    std::string_view base_name,
    Leafs<is_mode<MODE::VARIANT, mode>()> leafs,
    additional_args_t<mode, pl_mode> additional_args
) {
    using T = lexer::StructField;
    uint64_t outer_array_length;
    uint32_t* array_lengths;
    uint8_t array_depth;
    if constexpr (is_mode<MODE::ARRAY, mode>()) {
        outer_array_length = 1;
        array_depth = 1;
    } else if constexpr (pl_mode == MODE::FIXED_ARRAY ||  (pl_mode == MODE::DEFAULT && is_mode<MODE::FIXED_ARRAY, mode>())) {
        outer_array_length = additional_args.outer_array_length;
        array_lengths = additional_args.array_lengths;
        array_depth = additional_args.array_depth;
    } else {
        outer_array_length = 1;
        array_depth = 0;
    }

    lexer::FIELD_TYPE _type = field_type->type;
    switch (_type)
    {
    case lexer::FIELD_TYPE::STRING_FIXED: {
        auto fixed_string_type = field_type->as_fixed_string();
        uint32_t length = fixed_string_type->length;
        auto size_type_str =  get_size_type_str(length);
        auto chunk_start_ptr = leafs.template reserve_size8<mode>(outer_array_length * length);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto unique_name = get_unique_name<mode, pl_mode, "String">(additional_args);

        auto string_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();
        
        auto c_str_method = string_struct
        .method("const char*", "c_str");
        if constexpr (is_array_element<pl_mode>) {
            c_str_method = c_str_method
            .line("return reinterpret_cast<const char*>(base + ", chunk_start_ptr, " + ", make_idx_calc<false>(array_lengths, array_depth), " * ", length, ");");
        } else {
            if (array_depth == 0) {
                c_str_method = c_str_method
                .line("return reinterpret_cast<const char*>(base + ", chunk_start_ptr, ");");
            } else {
                c_str_method = c_str_method
                .line("return reinterpret_cast<const char*>(base + ", chunk_start_ptr, " + ", make_idx_calc<false>(array_lengths, array_depth), " * ", length, ");");
            }
        }

        string_struct = c_str_method
            .end()
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
        
        code = string_struct
        .end();

        if constexpr (is_array_element<pl_mode>) {
            code = code
            .method(unique_name, "get", codegen::Args("uint32_t idx"))
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            code = code
            .method(unique_name, get_name<mode, pl_mode>(additional_args, unique_name))
                .line(array_ctor_strs.ctor_used)
            .end();
        }
        return {(T*)(fixed_string_type + 1), code};
    }
    case lexer::FIELD_TYPE::STRING: {
        if constexpr (is_mode<MODE::ARRAY, mode>() || is_mode<MODE::FIXED_ARRAY, mode>()) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
        auto string_type = field_type->as_string();
        lexer::SIZE size_size = string_type->size_size;
        lexer::SIZE stored_size_size = string_type->stored_size_size;
        auto size_type_str = get_size_type_str(size_size);
        auto chunk_start_ptr = leafs.template reserve_size8<MODE::ARRAY>(1);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        uint16_t leaf_idx = leafs.template reserve_idx<MODE::DEFAULT>(stored_size_size);
        printf("STRING.size leaf idx: %d\n", leaf_idx);
        leafs.cc->sizes()[leaf_idx] = stored_size_size;

        uint16_t size_leaf_idx = leafs.next_size_leaf_idx();
        printf("STRING size_leaf_idx: %d\n", size_leaf_idx);
        leafs.size_leafs()[size_leaf_idx] = {leaf_idx, string_type->min_length, size_size, stored_size_size};

        auto unique_name = get_unique_name<mode, pl_mode>(additional_args);

        code = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end()
            .method("const char*", "c_str")
                .line("return add_offset<const char>(base, ", chunk_start_ptr, ");")
            .end()
            .method(size_type_str, "size")
                .line("return ", base_name, "::size", size_leaf_idx, "(base);")
            .end()
            .method(size_type_str, "length")
                .line("return size() - 1;")
            .end()
            ._private()
            .field("size_t", "base")
        .end()
        .method(unique_name, get_name<mode, pl_mode>(additional_args, unique_name))
            .line(array_ctor_strs.ctor_used)
        .end();

        return {(T*)(string_type + 1), code};
        }
    }
    case lexer::FIELD_TYPE::ARRAY_FIXED: {
        auto array_type = field_type->as_array();
        uint32_t length = array_type->length;
        auto size_type_str = get_size_type_str(length);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        uint16_t depth;
        if constexpr (pl_mode == MODE::FIXED_ARRAY) {
            depth = additional_args.depth + 1;
        } else {
            depth = 0;
        }

        auto unique_name = get_unique_name<mode, pl_mode>(additional_args, [depth]() { return codegen::StringParts{"Array_"_sl, depth}; });

        codegen::NestedStruct<codegen::__UnknownStruct> array_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        uint32_t new_array_lengths[array_depth];
        if (array_depth > 0) {
            uint8_t last = array_depth - 1;
            for (uint8_t i = 0; i < last; i++) {
                new_array_lengths[i] = array_lengths[i];
            }
            new_array_lengths[last] = length;
        }

        auto result = gen_leaf<static_cast<MODE>(mode | MODE::FIXED_ARRAY), MODE::FIXED_ARRAY>(
            array_type->inner_type(),
            buffer,
            codegen::__UnknownStruct{array_struct},
            base_name,
            leafs,
            GenFixedArrayLeafArgs{array_type->length * outer_array_length, new_array_lengths, depth, static_cast<uint8_t>(array_depth + 1)}
            );

        array_struct = codegen::NestedStruct<codegen::__UnknownStruct>{result.code}
            .method(codegen::Attributes("constexpr"), size_type_str, "length")
                .line("return ", length, ";")
            .end()
            ._private()
            .field("size_t", "base");

        for (uint8_t i = 0; i < array_depth; i++) {
            array_struct = array_struct
            .field("uint32_t", codegen::StringParts("idx_", i));
        }

        code = array_struct
        .end();
        
        if constexpr (is_array_element<pl_mode>) {
            code = code
            .method(unique_name, "get", codegen::Args("uint32_t idx"))
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            code = code
            .method(unique_name, get_name<mode, pl_mode>(additional_args, unique_name))
                .line(array_ctor_strs.ctor_used)
            .end();
        }
        return {result.next, code};
    }
    case lexer::FIELD_TYPE::ARRAY: {
        if constexpr (is_mode<MODE::ARRAY, mode>() || is_mode<MODE::FIXED_ARRAY, mode>()) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
        auto array_type = field_type->as_array();
        lexer::SIZE size_size = array_type->size_size;
        lexer::SIZE stored_size_size = array_type->stored_size_size;
        auto size_type_str = get_size_type_str(size_size);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(0);

        auto unique_name = get_unique_name<mode, pl_mode>(additional_args);

        auto array_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        auto result = gen_leaf<static_cast<MODE>(mode | MODE::ARRAY), MODE::ARRAY>(
            array_type->inner_type(),
            buffer,
            codegen::__UnknownStruct{array_struct},
            base_name,
            leafs,
            GenArrayLeafArgs{}
        );

        uint16_t leaf_idx = leafs.template reserve_idx<MODE::DEFAULT>(stored_size_size);
        leafs.cc->sizes()[leaf_idx] = stored_size_size;

        uint16_t size_leaf_idx = leafs.next_size_leaf_idx();
        leafs.size_leafs()[size_leaf_idx] = {leaf_idx, array_type->length, size_size, stored_size_size};

        code = codegen::NestedStruct<codegen::__UnknownStruct>{result.code}
            .method(size_type_str, "length")
                .line("return ", base_name, "::size", size_leaf_idx, "(base);")
            .end()
            ._private()
            .field("size_t", "base")
            .field("uint32_t", "idx_0")
        .end()
        .method(unique_name, get_name<mode, pl_mode>(additional_args, unique_name))
            .line(array_ctor_strs.ctor_used)
        .end();

        return {result.next, code};
        }
    }
    case lexer::FIELD_TYPE::VARIANT: {
        auto variant_type = field_type->as_variant();
        uint16_t variant_count = variant_type->variant_count;

        CodeChunks *cc = leafs.cc;

        auto unique_name = get_unique_name<mode, pl_mode, "Variant">(additional_args);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);
        
        auto variant_struct = code
        ._struct(unique_name);

        if (array_depth == 0) {
            variant_struct = variant_struct
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();
        } else {
            std::string ctor_args = "size_t prev_base, " + std::string(array_ctor_strs.ctor_args);
            std::string ctor_inits = "prev_base(prev_base), " + std::string(array_ctor_strs.ctor_inits);
            variant_struct = variant_struct
            .ctor(ctor_args, ctor_inits).end();
        }
        
        printf("[variant_id] outer_array_length: %d\n", outer_array_length);
        if (variant_count <= UINT8_MAX) {
            Buffer::Index<char>* chunk_start_ptr = leafs.template reserve_size8<mode>(outer_array_length);
            auto id_get_method = variant_struct
            .method("uint8_t", "id");
            if (array_depth == 0) {
                id_get_method = id_get_method
                .line("return *add_offset<uint8_t>(base, ", chunk_start_ptr, ");");
            } else {
                id_get_method = id_get_method
                .line("return *add_offset<uint8_t>(prev_base, ", chunk_start_ptr, " + ", make_idx_calc<true>(array_lengths, array_depth), ");");
            }
            variant_struct = id_get_method
            .end();
        } else {
            Buffer::Index<char>* chunk_start_ptr = leafs.template reserve_size16<mode>(outer_array_length);
            auto id_get_method = variant_struct
            .method("uint8_t", "id");
            if (array_depth == 0) {
                id_get_method = id_get_method
                .line("return *add_offset<uint16_t>(base, ", chunk_start_ptr, ");");
            } else {
                id_get_method = id_get_method
                .line("return *add_offset<uint16_t>(prev_base, ", chunk_start_ptr, " + ", make_idx_calc<false>(array_lengths, array_depth), " * 2);");
            }
            variant_struct = id_get_method
            .end();
        }              

        auto type = variant_type->first_variant();

        uint16_t current_variant_idx = cc->current_variant_base_idx;
        uint16_t start_variant_idx = current_variant_idx;
        uint16_t end_variant_idx = current_variant_idx + variant_type->level_variant_leafs;
        cc->current_variant_base_idx = end_variant_idx;

        printf("variant idx range: %d - %d\n", current_variant_idx, cc->current_variant_base_idx);
        uint64_t max_offset = 0;

        uint16_t variant_depth;
        if constexpr (pl_mode == MODE::VARIANT) {
            variant_depth = additional_args.variant_depth + 1;
        } else {
            variant_depth = 0;
        }
        
        for (uint16_t i = 0; i < variant_count; i++) {
            auto type_meta = variant_type->type_metas()[i];
            auto fixed_leaf_counts = type_meta.leaf_counts.counts;
            auto var_leaf_counts = lexer::LeafCounts{0}.counts;
            auto variant_field_counts = type_meta.variant_field_counts.counts;
            uint16_t total_fixed_leafs = fixed_leaf_counts.total();
            uint16_t total_var_leafs = var_leaf_counts.total();
            uint16_t level_variant_fields = variant_field_counts.total();
            uint16_t total_size_leafs = 0;

            uint8_t data[Leafs<true>::mem_size(total_var_leafs, total_size_leafs, level_variant_fields)];
            Leafs<true> variant_leafs = {
                cc,
                data,
                fixed_leaf_counts,
                var_leaf_counts,
                variant_field_counts,
                total_fixed_leafs,
                total_var_leafs,
                total_size_leafs,
                level_variant_fields,
                current_variant_idx
            };
            auto result = gen_leaf<static_cast<MODE>(mode | MODE::VARIANT), MODE::VARIANT>(
                type,
                buffer,
                codegen::__UnknownStruct{variant_struct},
                base_name,
                variant_leafs,
                GenVariantLeafArgs{i, variant_depth}
            );
            variant_struct = codegen::NestedStruct<codegen::__UnknownStruct>{result.code};
            type = (lexer::Type*)result.next;

            uint64_t offset = 0;
            offset = set_sizes(
                offset,
                fixed_leaf_counts,
                variant_field_counts,
                [&variant_leafs, current_variant_idx](uint16_t i, uint64_t offset) {
                    uint16_t cc_idx = i + current_variant_idx;
                    uint64_t size = variant_leafs.cc->sizes()[cc_idx];
                    variant_leafs.cc->sizes()[cc_idx] = offset;
                    return offset + size;
                },
                [&variant_leafs](uint16_t i, uint64_t offset) {
                    auto variant_field = variant_leafs.variant_fields()[i];
                    for (uint16_t cc_idx = variant_field.start; cc_idx < variant_field.end; cc_idx++) {
                        printf("Adding varaint base of %d to leaf cc_idx: %d\n", offset, cc_idx);
                        variant_leafs.cc->sizes()[cc_idx] += offset;
                    }
                    return offset + variant_field.size;
                }
            );
            max_offset = std::max(max_offset, offset);

            current_variant_idx = *variant_leafs.current_variant_idx();
        }

        VariantField* variant_field_ptr = leafs.reserve_variant_field(variant_type->max_alignment);
        uint64_t max_size;
        if (array_depth == 0) {
            max_size = max_offset;
        } else {
            max_size = next_multiple(max_offset, variant_type->max_alignment) * outer_array_length;
        }
        *variant_field_ptr =  {max_size, start_variant_idx, cc->current_variant_base_idx};

        printf("cc->current_variant_base_idx: %d, max_offset: %d, start_variant_idx: %d, current_variant_idx: %d, end_variant_idx: %d\n", cc->current_variant_base_idx, max_offset, start_variant_idx, current_variant_idx, end_variant_idx);

        variant_struct = variant_struct
        ._private()
            .field("size_t", "base");

        if (array_depth > 0) {
            variant_struct = variant_struct
            .field("size_t", "prev_base");
        }
        for (uint8_t i = 0; i < array_depth; i++) {
            variant_struct = variant_struct
            .field("uint32_t", codegen::StringParts("idx_", i));
        }

        code = variant_struct
        .end();

        if constexpr (is_array_element<pl_mode>) {
            code = code
            .method("Variant", "get", codegen::Args("uint32_t idx"))
                .line("return {base, base + ", make_idx_calc<false, true>(array_lengths, array_depth), " * ", max_size, std::string_view{array_ctor_strs.el_ctor_used.data() + 12, array_ctor_strs.el_ctor_used.length() - 12})
            .end();
        } else {
            auto variant_get_method = code
            .method(unique_name, get_name<mode, pl_mode>(additional_args, unique_name));
            if (array_depth == 0) {
                variant_get_method = variant_get_method
                .line(array_ctor_strs.ctor_used);
            } else {
                variant_get_method = variant_get_method
                .line("return {base, base + ", make_idx_calc<false>(array_lengths, array_depth), " * ", max_size, std::string_view{array_ctor_strs.ctor_used.data() + 12, array_ctor_strs.ctor_used.length() - 12});
            }
            code = variant_get_method
            .end();
        }

        return {(lexer::StructField*)(type), code};
    }
    case lexer::FIELD_TYPE::PACKED_VARIANT: {
        printf("[ERROR] PACKED_VARIANT not supported.\n");
        exit(1);
    }
    case lexer::FIELD_TYPE::DYNAMIC_VARIANT: {
        printf("[ERROR] DYNAMIC_VARIANT not supported.\n");
        exit(1);
        if constexpr (is_mode<MODE::ARRAY, mode>() || is_mode<MODE::FIXED_ARRAY, mode>()) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        } else {
        }
    }
    case lexer::FIELD_TYPE::IDENTIFIER: {
        auto identified_type = field_type->as_identifier();
        auto identifier = buffer.get(identified_type->identifier_idx);
        if (identifier->keyword != lexer::KEYWORDS::STRUCT) {
            INTERNAL_ERROR("not implemented");
        }
        auto struct_type = identifier->data()->as_struct();

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto unique_name = get_unique_name<mode, pl_mode>(additional_args, [&struct_type]() { return std::string_view{struct_type->name.offset, struct_type->name.length}; });
        
        codegen::NestedStruct<codegen::__UnknownStruct> struct_code = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctor_args, array_ctor_strs.ctor_inits).end();

        auto field = struct_type->first_field();
        for (uint16_t i = 0; i < struct_type->field_count; i++) {
            auto field_data = field->data();
            auto name = std::string_view{field_data->name.offset, field_data->name.length};
            uint16_t struct_depth;
            if constexpr (pl_mode == MODE::DEFAULT) {
                struct_depth = additional_args.depth + 1;
            } else {
                struct_depth = 0;
            }

            GenStructLeafArgs<mode> args;
            if constexpr (is_mode<MODE::FIXED_ARRAY, mode>()) {
                args = GenStructLeafArgsInArray{name, outer_array_length, array_lengths, struct_depth, array_depth};
            } else {
                args = GenStructLeafArgsDefault{name, struct_depth};
            }
            
            auto result = gen_leaf<mode, MODE::DEFAULT>(
                field_data->type(),
                buffer,
                codegen::__UnknownStruct{struct_code},
                base_name,
                leafs,
                args
            );
            field = result.next;
            struct_code = codegen::NestedStruct<codegen::__UnknownStruct>{result.code};
        }

        struct_code = struct_code
            ._private()
            .field("size_t", "base");
        
        for (uint8_t i = 0; i < array_depth; i++) {
            struct_code = struct_code
            .field("uint32_t", codegen::StringParts("idx_", i));
        }

        code = struct_code
        .end();

        if constexpr (is_array_element<pl_mode>) {
            code = code
            .method(unique_name, "get", codegen::Args("uint32_t idx"))
                .line(array_ctor_strs.el_ctor_used)
            .end();
        } else {
            code = code
            .method(unique_name, get_name<mode, pl_mode>(additional_args, unique_name))
                .line(array_ctor_strs.ctor_used)
            .end();
        }

        return {(T*)(identified_type + 1), code};
    }
    default: {
        struct TypeInfo {
            std::string name;
            lexer::SIZE size;
            std::string size_str;
        };
        static constexpr TypeInfo type_infos[] = {
            {"bool",        lexer::SIZE_1, "1"},
            {"uint8_t",     lexer::SIZE_1, "1"},
            {"uint16_t",    lexer::SIZE_2, "2"},
            {"uint32_t",    lexer::SIZE_4, "4"},
            {"uint64_t",    lexer::SIZE_8, "8"},
            {"int8_t",      lexer::SIZE_1, "1"},
            {"int16_t",     lexer::SIZE_2, "2"},
            {"int32_t",     lexer::SIZE_4, "4"},
            {"int64_t",     lexer::SIZE_8, "8"},
            {"float32_t",   lexer::SIZE_4, "4"},
            {"float64_t",   lexer::SIZE_8, "8"}
        };
        auto [type_name, type_size, type_size_str] = type_infos[_type];
        printf("outer_array_length: %d\n", outer_array_length);
        auto chunk_start_ptr = leafs.template reserve_next<mode>(type_size, outer_array_length);
        if constexpr (is_array_element<pl_mode>) {
            if (type_size == lexer::SIZE::SIZE_1) {
                code = code
                .method(type_name, "get", codegen::Args("uint32_t idx"))
                    .line("return *add_offset<", type_name, ">(base, ", chunk_start_ptr, " + ", make_idx_calc<true ,true>(array_lengths, array_depth), ");")
                .end();
            } else {
                code = code
                .method(type_name, "get", codegen::Args("uint32_t idx"))
                    .line("return *add_offset<", type_name, ">(base, ", chunk_start_ptr, " + ", make_idx_calc<false, true>(array_lengths, array_depth), " * ", type_size_str, ");")
                .end();
            }
        } else {
            auto name = get_name<mode, pl_mode>(additional_args);
            if (array_depth == 0) {
                code = code
                .method(type_name, name)
                    .line("return *add_offset<", type_name, ">(base, ", chunk_start_ptr, ");")
                .end();
            } else {
                if (type_size == lexer::SIZE::SIZE_1) {
                    code = code
                    .method(type_name, name)
                        .line("return *add_offset<", type_name, ">(base, ", chunk_start_ptr, " + ", make_idx_calc<true>(array_lengths, array_depth), ");")
                    .end();
                } else {
                    code = code
                    .method(type_name, name)
                        .line("return *add_offset<", type_name, ">(base, ", chunk_start_ptr, " + ", make_idx_calc<false>(array_lengths, array_depth), " * ", type_size_str, ");")
                    .end();
                }
            }
        }
        return {(T*)(field_type + 1), code};
    }
    }
}

constexpr size_t log2(size_t n){
  return ( (n < 2) ? 0 : 1 + log2(n >> 1));
}

static inline void handled_write (int fd, const char* buf, size_t size) {
    int result = write(fd, buf, size);
    if (result == -1) {
        printf("write failed, errno: %d\n", result, errno);
        exit(1);
    }
}

template <size_t size>
requires (is_power_of_two(size))
struct Writer {
    char buffer[size];
    size_t position = 0;
    int fd;
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
        handled_write(fd, buffer, position);
        position = 0;
    }

    ~Writer () {
        // Auto call done
        done();
    }
};

void print_leafs (const char* name, lexer::LeafCounts leafs) {
    printf("%s: {8: %u, 16: %u, 32: %u, 64: %u, total: %u}\n", name, leafs.counts.size8, leafs.counts.size16, leafs.counts.size32, leafs.counts.size64, leafs.total());
}

void generate (lexer::StructDefinition* target_struct, Buffer& buffer, int output_fd) {
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
    uint16_t total_variant_leafs = target_struct->total_variant_leafs;
    uint16_t total_leafs = total_fixed_leafs + total_var_leafs + total_variant_leafs;
    uint16_t variant_base_idx = total_fixed_leafs + total_var_leafs;
    uint16_t total_size_leafs = target_struct->size_leafs_count;
    print_leafs("fixed_leaf_counts", fixed_leaf_counts);
    print_leafs("var_leaf_counts", var_leaf_counts);
    print_leafs("variant_field_counts", variant_field_counts);
    printf("total_variant_leafs: %u\n", total_variant_leafs);
    printf("total_leafs: %u\n", total_leafs);
    printf("total_size_leafs: %u\n", total_size_leafs);

    uint8_t _mem[CodeChunks::mem_size(total_leafs)];
    CodeChunks* code_chunks = CodeChunks::create(
        _mem,
        total_leafs,
        variant_base_idx
    );

    uint8_t data[Leafs<false>::mem_size(total_var_leafs, total_size_leafs, level_variant_fields)];
    Leafs<false> leafs = {
        code_chunks,
        data,
        fixed_leaf_counts,
        var_leaf_counts,
        variant_field_counts,
        total_fixed_leafs,
        total_var_leafs,
        total_size_leafs,
        level_variant_fields,
        0
    };
    
    std::string_view struct_name = std::string_view{target_struct->name.offset, target_struct->name.length};
    auto struct_code = code
    ._struct(struct_name)
        .ctor("size_t base", "base(base)").end();

    auto field = target_struct->first_field();
    for (uint16_t i = 0; i < target_struct->field_count; i++) {
        auto field_data = field->data();
        auto name = std::string_view{field_data->name.offset, field_data->name.length};
        auto result = gen_leaf<MODE::DEFAULT, MODE::DEFAULT>(
            field_data->type(),
            buffer,
            codegen::__UnknownStruct{struct_code},
            struct_name,
            leafs,
            GenStructLeafArgsDefault{name, 0}
        );
        field = result.next;
        struct_code = codegen::Struct<decltype(struct_code)::__Last>{result.code};
    }

    struct_code = struct_code
    ._private()
    .field("size_t", "base");

    for (uint16_t i = 0; i < total_size_leafs; i++) {
        auto [leaf_idx, min_size, size_size, stored_size_size] = leafs.size_leafs()[i];
        uint16_t map_idx = code_chunks->current_map_idx++;
        printf("size_leaf map_idx: %d, leaf_idx: %d\n", map_idx, leaf_idx);
        code_chunks->chunk_map()[map_idx] = leaf_idx;
        struct_code = struct_code
        .method(codegen::Attributes("static"), get_size_type_str(size_size), codegen::StringParts("size", i), codegen::Args("size_t base"))
            .line("return *add_offset<", get_size_type_str(stored_size_size), ">(base, ", code_chunks->chunk_starts() + leaf_idx, ");")
        .end();
    }

    auto code_done = struct_code
    .end()
    .end();
 
    uint64_t offset = 0;
    // printf ("offset: %d\n", offset);

    offset = set_sizes(
        offset,
        fixed_leaf_counts,
        variant_field_counts,
        [&code_chunks](uint16_t i, uint64_t offset) {
            uint64_t* size_ptr = code_chunks->sizes() + i;
            uint64_t size = *size_ptr;
            printf("i: %d, size: %d\n", i, size);
            *size_ptr = offset;
            return offset + size;
        },
        [&leafs, &code_chunks](uint16_t i, uint64_t offset) {
            auto variant_field = leafs.variant_fields()[i];
            for (uint16_t cc_idx = variant_field.start; cc_idx < variant_field.end; cc_idx++) {
                printf("Adding varaint base of %d to leaf cc_idx: %d\n", offset, cc_idx);
                code_chunks->sizes()[cc_idx] += offset;
            }
            printf("i: %d, variant_field.size: %d\n", i, variant_field.size);
            return offset + variant_field.size;
        }
    );
     
    // printf ("offset: %d\n", offset);

    if (total_var_leafs > 0) {
        uint8_t max_var_leaf_align;
        if (var_leaf_counts.size64 > 0) {
            max_var_leaf_align = 8;
        } else if (var_leaf_counts.size32 > 0) {
            max_var_leaf_align = 4;
        } else if (var_leaf_counts.size16 > 0) {
            max_var_leaf_align = 2;
        } else {
            goto done;
        }
        {
            // Add padding based on alignment
            size_t mod = offset % max_var_leaf_align;
            size_t padding = (max_var_leaf_align - mod) & (max_var_leaf_align - 1);
            offset += padding;
        }
        done:;
    }
    std::string last_fixed_offset_str = std::to_string(offset);

    for (uint16_t j = 0; j < total_leafs; j++) {
        uint16_t i = code_chunks->chunk_map()[j];
        printf("chunk_map[%d]: %d\n", j, i);
    }

    auto writer = Writer<4096>(output_fd);

    
    
    char* last_offset_str = code_done.buffer().get<char>({0});
    for (uint16_t j = 0; j < total_leafs; j++) {
        uint16_t i = code_chunks->chunk_map()[j];
        auto chunk_start = code_chunks->chunk_starts()[i];
        auto offset_str = code_done.buffer().get(chunk_start);
        
        char end_backup = *offset_str;
        *offset_str = 0;
        writer.write(last_offset_str, offset_str);
        *offset_str = end_backup;
        last_offset_str = offset_str;
        
        if (i < total_fixed_leafs) {
            writer.write(std::to_string(code_chunks->sizes()[i]));
        } else if (i < total_fixed_leafs + total_var_leafs) {
            writer.write(last_fixed_offset_str);
            uint64_t size_leafs[total_size_leafs];
            memset(size_leafs, 0, sizeof(size_leafs));
            uint16_t known_size_leafs = 0;
            for (uint16_t h = total_fixed_leafs; h < i; h++) {
                uint16_t size_leaf_idx = leafs.size_leafe_idxs()[h - total_fixed_leafs];
                uint64_t* size_leaf = size_leafs + size_leaf_idx;
                if (*size_leaf == 0) {
                    known_size_leafs++;
                }
                *size_leaf += code_chunks->sizes()[h];
            }
            for (uint16_t size_leaf_idx = 0; size_leaf_idx < known_size_leafs; size_leaf_idx++) {
                writer.write(" + ");
                writer.write(struct_name);
                writer.write("::size");
                writer.write(std::to_string(size_leaf_idx));
                uint64_t size = size_leafs[size_leaf_idx];
                if (size == 1) {
                    writer.write("(base)");
                } else {
                    writer.write("(base) * ");
                    writer.write(std::to_string(size));
                }
            }
        } else {
            printf("i: %d, size: %d\n", i, code_chunks->sizes()[i]);
            writer.write(std::to_string(code_chunks->sizes()[i]));
        }
    }
    writer.write(last_offset_str, code_done.buffer().get<char>({code_done.buffer().current_position()}));
    writer.done();


}


}