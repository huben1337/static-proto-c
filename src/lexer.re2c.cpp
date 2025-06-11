#pragma once

#include "base.cpp"
#include "fatal_error.cpp"
#include "lexer_types.cpp"
#include "lex_result.cpp"
#include "lex_error.cpp"
#include "lex_helpers.re2c.cpp"
#include "parse_int.re2c.cpp"
#include "string_helpers.cpp"
#include <cstdint>
#include <string_view>
#include "logger.cpp"

namespace lexer {

/*!re2c
    re2c:define:YYMARKER = YYCURSOR;
    re2c:yyfill:enable = 0;
    re2c:define:YYCTYPE = char;

    any_white_space = [ \t\r\n];
    white_space = [ \t];
*/

template <typename F, typename G>
INLINE char* lex_range_argument (char* YYCURSOR, F on_fixed, G on_range) {

    YYCURSOR = skip_white_space(YYCURSOR);
    
    auto firstArgPos = YYCURSOR;
    auto parsed_0 = parse_uint<uint32_t>(YYCURSOR);
    YYCURSOR = parsed_0.cursor;
    auto min = parsed_0.value;

    YYCURSOR = skip_white_space(YYCURSOR);

    if (*YYCURSOR == '.') {
        YYCURSOR++;
        if (*YYCURSOR != '.') {
            UNEXPECTED_INPUT("expected '..' to mark range");
        }
        
        YYCURSOR = skip_white_space(YYCURSOR + 1);

        auto parsed_1 = parse_uint<uint32_t>(YYCURSOR);
        YYCURSOR = parsed_1.cursor;
        auto max = parsed_1.value;
        if (max <= min) show_syntax_error("invalid range", firstArgPos, YYCURSOR - 1);
        on_range(Range{min, max});
    } else {
        on_fixed(min);
    }

    /*!local:re2c
        white_space* ">" { goto done; }
        * { UNEXPECTED_INPUT("expected end of argument list"); }
    */
    done:
    return YYCURSOR;
}

template <typename T>
requires (std::is_integral_v<T>)
INLINE LexResult<T> lex_attribute_value (char* YYCURSOR) {
    YYCURSOR = lex_same_line_symbol<'=', "Expected value assignment for attribute">(YYCURSOR);
    YYCURSOR = skip_white_space(YYCURSOR);
    return parse_uint<uint64_t>(YYCURSOR);
}

struct VariantAttributes {
    uint64_t max_wasted_bytes;
    uint64_t shared_id;
};

INLINE LexResult<VariantAttributes> lex_variant_attributes (char* YYCURSOR) {
    uint64_t max_wasted_bytes = 32;
    uint64_t shared_id;
    bool has_shared_id = false;
    bool has_max_wasted_bytes = false;

    char* YYMARKER = YYCURSOR;
    /*!local:re2c
        white_space* "[" { goto maybe_lex_attributes; }
        * { YYCURSOR = YYMARKER; goto lex_attributes_done; }
    */

    maybe_lex_attributes: {
        /*!local:re2c
            "[" {}
            * { show_syntax_error("unexpected character", YYCURSOR - 1); }
        */
        YYCURSOR = skip_white_space(YYCURSOR);
        
        lex_attributes: {
            /*!local:re2c
                "max_wasted" { goto lex_max_wasted_bytes; }
                "shared_id" { goto lex_shared_id; }
                * { show_syntax_error("unexpected attribute", YYCURSOR - 1); }
            */
            lex_max_wasted_bytes: {
                if (has_max_wasted_bytes) {
                    show_syntax_error("conflicting attributes", YYCURSOR - 1);
                }
                has_max_wasted_bytes = true;
                auto parsed = lex_attribute_value<uint64_t>(YYCURSOR);
                max_wasted_bytes = parsed.value;
                YYCURSOR = parsed.cursor;
                goto attribute_end;
            }
            lex_shared_id: {
                if (has_shared_id) {
                    show_syntax_error("conflicting attributes", YYCURSOR - 1);
                }
                has_shared_id = true;
                auto parsed = lex_attribute_value<uint64_t>(YYCURSOR);
                shared_id = parsed.value;
                YYCURSOR = parsed.cursor;
                goto attribute_end;
            }
            attribute_end: {
                /*!local:re2c
                    white_space* "]" { goto close_attributes; }
                    white_space* "," { goto lex_attributes; }
                    * { show_syntax_error("expected end of attributes or next ", YYCURSOR); }
                */
            }
        }

        close_attributes:
        /*!local:re2c
            "]" { goto lex_attributes_done; }
            * { show_syntax_error("expected closing of attributes", YYCURSOR); }
        */
    }
    lex_attributes_done: {
        return {YYCURSOR, {max_wasted_bytes, shared_id}};
    }
}

INLINE LexResult<std::string_view>  lex_identifier_name (char* YYCURSOR) {
    char* YYMARKER;
    /*!local:re2c
        re2c:define:YYMARKER = YYMARKER;
        
        white_space* [a-zA-Z_]              { goto name_start; }

        invalid = [^a-zA-Z0-9_];

        white_space* "int8"    invalid      { UNEXPECTED_INPUT("int8 is reserved");    }
        white_space* "int16"   invalid      { UNEXPECTED_INPUT("int16 is reserved");   }
        white_space* "int32"   invalid      { UNEXPECTED_INPUT("int32 is reserved");   }
        white_space* "int64"   invalid      { UNEXPECTED_INPUT("int64 is reserved");   }
        white_space* "uint8"   invalid      { UNEXPECTED_INPUT("uint8 is reserved");   }
        white_space* "uint16"  invalid      { UNEXPECTED_INPUT("uint16 is reserved");  }
        white_space* "uint32"  invalid      { UNEXPECTED_INPUT("uint32 is reserved");  }
        white_space* "uint64"  invalid      { UNEXPECTED_INPUT("uint64 is reserved");  }
        white_space* "float32" invalid      { UNEXPECTED_INPUT("float32 is reserved"); }
        white_space* "float64" invalid      { UNEXPECTED_INPUT("float64 is reserved"); }
        white_space* "bool"    invalid      { UNEXPECTED_INPUT("bool is reserved");    }
        white_space* "string"  invalid      { UNEXPECTED_INPUT("string is reserved");  }
        white_space* "array"   invalid      { UNEXPECTED_INPUT("array is reserved");   }
        white_space* "variant" invalid      { UNEXPECTED_INPUT("variant is reserved"); }
        white_space* "struct"  invalid      { UNEXPECTED_INPUT("struct is reserved");  }
        white_space* "enum"    invalid      { UNEXPECTED_INPUT("enum is reserved");    }
        white_space* "union"   invalid      { UNEXPECTED_INPUT("union is reserved");   }
        white_space* "target"  invalid      { UNEXPECTED_INPUT("target is reserved");  }

        * { UNEXPECTED_INPUT("expected name"); }
    */
    name_start:
    auto start = YYCURSOR - 1;
    /*!local:re2c
        [a-zA-Z0-9_]*  { goto name_end; }
    */
    name_end:
    auto end = YYCURSOR;

    return { YYCURSOR, {start, end} };   
}

INLINE void add_identifier (IdentifierMap &identifier_map, std::string_view name, IdentifedDefinitionIndex definition_idx) {
    bool did_emplace = identifier_map.emplace(std::pair{name, definition_idx}).second;
    if (!did_emplace) {
        show_syntax_error("identifier already defined", name.begin(), name.end());
    }
}

struct LexTypeResult {
    char* cursor;
    LeafCounts fixed_leaf_counts;
    LeafCounts var_leafs_count;
    LeafCounts variant_field_counts;
    uint64_t min_byte_size;
    uint64_t max_byte_size;
    uint16_t total_variant_count;
    uint16_t level_variant_fields;
    uint16_t total_variant_fixed_leafs;
    uint16_t total_variant_var_leafs;
    uint16_t level_size_leafs;
    SIZE alignment;
};

struct LexFixedTypeResult {
    char* cursor;
    LeafCounts fixed_leaf_counts;
    LeafCounts variant_field_counts;
    uint64_t byte_size;
    uint16_t total_variant_count;
    uint16_t level_variant_fields;
    uint16_t total_variant_fixed_leafs;
    SIZE alignment;
};

template <bool is_dynamic>
using _VariantTypeMeta = std::conditional_t<is_dynamic, DynamicVariantTypeMeta, FixedVariantTypeMeta>;

template <bool expect_fixed>
std::conditional_t<expect_fixed, LexFixedTypeResult, LexTypeResult> lex_type (char* YYCURSOR, Buffer &buffer, IdentifierMap &identifier_map);

template <bool is_dynamic, bool expect_fixed>
INLINE std::conditional_t<expect_fixed, LexFixedTypeResult, LexTypeResult> add_variant_type (
    char* YYCURSOR,
    uint64_t inner_min_byte_size,
    uint64_t inner_max_byte_size,
    Buffer& buffer,
	Buffer& type_meta_buffer,
    IdentifierMap& identifier_map,
    __CreateExtendedResult<DynamicVariantType, Type> created_variant_type,
    Buffer::Index<uint8_t> types_start_idx,
    variant_count_t variant_count,
    uint16_t total_variant_count,
    uint16_t total_variant_fixed_leafs,
    uint16_t total_variant_var_leafs,
    uint16_t level_fixed_leafs,
    uint16_t level_var_leafs,
    uint16_t level_variant_fields,
    SIZE max_alignment
) {
    auto attribute_lex_result = lex_variant_attributes(YYCURSOR);
    YYCURSOR = attribute_lex_result.cursor;
    auto [max_wasted_bytes, shared_id] = attribute_lex_result.value;

    auto types_end_idx = buffer.position_idx<uint8_t>();

    size_t type_metas_mem_size = variant_count * sizeof(_VariantTypeMeta<is_dynamic>);
    buffer.next_multi_byte<void>(type_metas_mem_size);

    uint8_t* types_start_ptr = buffer.get(types_start_idx);
    uint8_t* types_end_ptr = buffer.get(types_end_idx);
    // based on the way the memory overlaps we can reverse copy
    uint8_t* types_src = types_end_ptr - 1;
    uint8_t* types_dst = types_end_ptr - 1 + type_metas_mem_size;
    while (types_src >= types_start_ptr) {
        *(types_dst--) = *(types_src--);
    }
    
    // printf("aligned: %d", (size_t)types_start_ptr % 8); - use this to confirm alignment - now VariantType is 8 byte aligned so it should always be aligned
    auto leafs_count_src = type_meta_buffer.get<_VariantTypeMeta<!expect_fixed>>({0});
    auto leafs_count_dst = reinterpret_cast<_VariantTypeMeta<is_dynamic>*>(types_start_ptr);
    for (size_t i = 0; i < variant_count; i++) {
        if constexpr (expect_fixed) {
            static_assert(!is_dynamic);
            *leafs_count_dst = *leafs_count_src;
        } else if constexpr (is_dynamic) {
            *leafs_count_dst = *leafs_count_src;
        } else {
            *leafs_count_dst = {leafs_count_src->fixed_leaf_counts, leafs_count_src->variant_field_counts};
        }
        leafs_count_dst++;
        leafs_count_src++;
    }

    type_meta_buffer.dispose();

    if constexpr (is_dynamic) {
        logger::debug("Lexer found DYNAMIC_VARIANT");
        buffer.get(created_variant_type.base)->type = DYNAMIC_VARIANT;
    } else {
        if ((inner_max_byte_size - inner_min_byte_size) > max_wasted_bytes) {
            logger::debug("Packing variant to satisfy size requirements");
            buffer.get(created_variant_type.base)->type = PACKED_VARIANT;
        } else {
            buffer.get(created_variant_type.base)->type = FIXED_VARIANT;
        }
    }

    LeafCounts variant_field_counts = {max_alignment};

    auto variant_type = buffer.get_aligned(created_variant_type.extended);
    variant_type->variant_count = variant_count;
    variant_type->max_alignment = max_alignment;
    variant_type->level_fixed_leafs = level_fixed_leafs;
    variant_type->level_var_leafs = level_var_leafs;

    LeafCounts fixed_leaf_counts;
    inner_max_byte_size = next_multiple(inner_max_byte_size, max_alignment);
    
    uint64_t max_byte_size = inner_max_byte_size;
    uint64_t min_byte_size = inner_min_byte_size;
    if (variant_count <= UINT8_MAX) {
        fixed_leaf_counts = {1, 0, 0, 0};
        max_byte_size += 1;
        min_byte_size += 1;
    } else {
        fixed_leaf_counts = {0, 1, 0, 0};
        max_byte_size += 2;
        min_byte_size += 2;
    }
    
    if constexpr (is_dynamic) {
        uint64_t max = inner_max_byte_size;
        uint64_t delta = inner_max_byte_size - inner_min_byte_size;
        SIZE stored_size_size;
        SIZE size_size;
        if (delta <= UINT8_MAX) {
            fixed_leaf_counts += {1, 0, 0, 0};
            max_byte_size += 1;
            stored_size_size = SIZE::SIZE_1;
            if (max <= UINT8_MAX) {
                size_size = SIZE::SIZE_1;
            } else if (max <= UINT16_MAX) {
                size_size = SIZE::SIZE_2;
            } else if (max <= UINT32_MAX) {
                size_size = SIZE::SIZE_4;
            } else {
                size_size = SIZE::SIZE_8;
            }
        } else if (delta <= UINT16_MAX) {
            fixed_leaf_counts += {0, 1, 0, 0};
            max_byte_size += 2;
            stored_size_size = SIZE::SIZE_2;
            if (max <= UINT16_MAX) {
                size_size = SIZE::SIZE_2;
            } else if (max <= UINT32_MAX) {
                size_size = SIZE::SIZE_4;
            } else {
                size_size = SIZE::SIZE_8;
            }
        } else if (delta <= UINT32_MAX) {
            fixed_leaf_counts += {0, 0, 1, 0};
            max_byte_size += 4;
            stored_size_size = SIZE::SIZE_4;
            size_size = SIZE::SIZE_4;
            if (max <= UINT32_MAX) {
                size_size = SIZE::SIZE_4;
            } else {
                size_size = SIZE::SIZE_8;
            }
        } else {
            fixed_leaf_counts += {0, 0, 0, 1};
            max_byte_size += 8;
            stored_size_size = SIZE::SIZE_8;
            size_size = SIZE::SIZE_8;
        }
        variant_type->min_byte_size = inner_min_byte_size;
        variant_type->stored_size_size = stored_size_size;
        variant_type->size_size = size_size;
    }
    
    

    if constexpr (expect_fixed) {
        return LexFixedTypeResult{
            YYCURSOR,
            fixed_leaf_counts,
            variant_field_counts,
            max_byte_size,
            total_variant_count,
            1,
            total_variant_fixed_leafs,
            max_alignment
        };
    } else {
        if constexpr (is_dynamic) {
            return LexTypeResult{
                YYCURSOR,
                fixed_leaf_counts,
                variant_field_counts,
                {0},
                min_byte_size,
                max_byte_size,
                total_variant_count,
                1,
                total_variant_fixed_leafs,
                total_variant_var_leafs,
                1,
                max_alignment
            };
        } else {
            return LexTypeResult{
                YYCURSOR,
                fixed_leaf_counts,
                {0},
                variant_field_counts,
                min_byte_size,
                max_byte_size,
                total_variant_count,
                1,
                total_variant_fixed_leafs,
                total_variant_var_leafs,
                0,
                max_alignment
            };
        }
    }
}

template <bool is_dynamic, bool expect_fixed>
INLINE std::conditional_t<expect_fixed, LexFixedTypeResult, LexTypeResult> lex_variant_types (
    char* YYCURSOR,
    uint64_t min_byte_size,
    uint64_t max_byte_size,
    Buffer& buffer,
	Buffer& type_meta_buffer,
    IdentifierMap& identifier_map,
    __CreateExtendedResult<DynamicVariantType, Type> created_variant_type,
    Buffer::Index<uint8_t> types_start_idx,
    variant_count_t variant_count,
    uint16_t inner_total_variant_count,
    uint16_t total_variant_fixed_leafs,
    uint16_t total_variant_var_leafs,
    uint16_t level_fixed_leafs,
    uint16_t level_var_leafs,
    uint16_t level_variant_fields,
    SIZE max_alignment
) {
    while (1) {
        variant_count++;
        YYCURSOR = skip_white_space(YYCURSOR);
        auto result = lex_type<expect_fixed>(YYCURSOR, buffer, identifier_map);
        YYCURSOR = result.cursor;
        inner_total_variant_count += result.total_variant_count;

        uint16_t inner_fixed_leafs;
        if constexpr (expect_fixed) {
            *type_meta_buffer.get_next<FixedVariantTypeMeta>() = {result.fixed_leaf_counts, result.variant_field_counts};
            inner_fixed_leafs = result.fixed_leaf_counts.total();
        } else {
            *type_meta_buffer.get_next<DynamicVariantTypeMeta>() = {result.fixed_leaf_counts, result.var_leafs_count, result.variant_field_counts, result.level_size_leafs};
            inner_fixed_leafs = result.fixed_leaf_counts.total();
            uint16_t inner_var_leafs = result.var_leafs_count.total();
            total_variant_var_leafs += inner_var_leafs + result.total_variant_var_leafs;
            level_var_leafs += inner_var_leafs;
        }

        total_variant_fixed_leafs += inner_fixed_leafs + result.total_variant_fixed_leafs;

        level_fixed_leafs += inner_fixed_leafs;

        level_variant_fields += result.level_variant_fields;

        max_alignment = std::max(max_alignment, result.alignment);
        if constexpr (expect_fixed) {
            uint64_t byte_size = result.byte_size;
            min_byte_size = std::min(min_byte_size, byte_size);
            max_byte_size = std::max(max_byte_size, byte_size);
        } else {
            min_byte_size = std::min(min_byte_size, result.min_byte_size);
            max_byte_size = std::max(max_byte_size, result.max_byte_size);
        }
        

        if constexpr (!is_dynamic && !expect_fixed) {
            if (result.var_leafs_count.as_uint64 > 0) {
                /*!local:re2c
                    white_space* "," { goto dynamic_variant_next; }
                    white_space* ">" { goto dynamic_variant_done;  }
                    * { UNEXPECTED_INPUT("expected ',' or '>'"); }
                */
                dynamic_variant_next: {
                    return lex_variant_types<true, expect_fixed>(
                        YYCURSOR,
                        min_byte_size,
                        max_byte_size,
                        buffer,
                        type_meta_buffer,
                        identifier_map,
                        created_variant_type,
                        types_start_idx,
                        variant_count,
                        variant_count + inner_total_variant_count,
                        total_variant_fixed_leafs,
                        total_variant_var_leafs,
                        level_fixed_leafs,
                        level_var_leafs,
                        level_variant_fields,
                        max_alignment
                    );
                }
                dynamic_variant_done: {
                    return add_variant_type<true, expect_fixed>(
                        YYCURSOR,
                        min_byte_size,
                        max_byte_size,
                        buffer,
                        type_meta_buffer,
                        identifier_map,
                        created_variant_type,
                        types_start_idx,
                        variant_count,
                        variant_count + inner_total_variant_count,
                        total_variant_fixed_leafs,
                        total_variant_var_leafs,
                        level_fixed_leafs,
                        level_var_leafs,
                        level_variant_fields,
                        max_alignment
                    );
                }
            }
        }
        
        /*!local:re2c
            white_space* "," { continue; }
            white_space* ">" { goto variant_done; }
            * { UNEXPECTED_INPUT("expected ',' or '>'"); }
        */
        variant_done: {
            return add_variant_type<is_dynamic, expect_fixed>(
                YYCURSOR,
                min_byte_size,
                max_byte_size,
                buffer,
                type_meta_buffer,
                identifier_map,
                created_variant_type,
                types_start_idx,
                variant_count,
                variant_count + inner_total_variant_count,
                total_variant_fixed_leafs,
                total_variant_var_leafs,
                level_fixed_leafs,
                level_var_leafs,
                level_variant_fields,
                max_alignment
            );
        }
    }
}


template <bool expect_fixed>
std::conditional_t<expect_fixed, LexFixedTypeResult, LexTypeResult> lex_type (char* YYCURSOR, Buffer &buffer, IdentifierMap &identifier_map) {
    const char* identifier_start = YYCURSOR;
    #define SIMPLE_TYPE(TYPE, ALIGN)                                                                                    \
    {                                                                                                                   \
        auto type = buffer.get_next<Type>();                                                                            \
        type->type = FIELD_TYPE::TYPE;                                                                                  \
        constexpr LeafCounts fixed_leaf_counts = {ALIGN};                                                               \
        constexpr uint64_t byte_size = byte_size_of(ALIGN);                                                             \
        if constexpr (expect_fixed) {                                                                                   \
            return LexFixedTypeResult{YYCURSOR, fixed_leaf_counts, {0}, byte_size, 0, 0, 0, ALIGN};                     \
        } else {                                                                                                        \
            return LexTypeResult{YYCURSOR, fixed_leaf_counts, {0}, {0}, byte_size, byte_size, 0, 0, 0, 0, 0, ALIGN};    \
        }                                                                                                               \
    }
    
    /*!local:re2c
        name = [a-zA-Z_][a-zA-Z0-9_]*;
                    
        "int8"      { SIMPLE_TYPE(INT8,    SIZE::SIZE_1 ) }
        "int16"     { SIMPLE_TYPE(INT16,   SIZE::SIZE_2 ) }
        "int32"     { SIMPLE_TYPE(INT32,   SIZE::SIZE_4 ) }
        "int64"     { SIMPLE_TYPE(INT64,   SIZE::SIZE_8 ) }
        "uint8"     { SIMPLE_TYPE(UINT8,   SIZE::SIZE_1 ) }
        "uint16"    { SIMPLE_TYPE(UINT16,  SIZE::SIZE_2 ) }
        "uint32"    { SIMPLE_TYPE(UINT32,  SIZE::SIZE_4 ) }
        "uint64"    { SIMPLE_TYPE(UINT64,  SIZE::SIZE_8 ) }
        "float32"   { SIMPLE_TYPE(FLOAT32, SIZE::SIZE_4 ) }
        "float64"   { SIMPLE_TYPE(FLOAT64, SIZE::SIZE_8 ) }
        "bool"      { SIMPLE_TYPE(BOOL,    SIZE::SIZE_1 ) }
        "string"    { goto string;                        }
        "array"     { goto array;                         }
        "variant"   { goto variant;                       }
        name        { goto identifier;                    }

        * { UNEXPECTED_INPUT("expected type"); }
    */
    #undef SIMPLE_TYPE
    #undef LEAF_COUNTS_TYPE_8
    #undef LEAF_COUNTS_TYPE_16
    #undef LEAF_COUNTS_TYPE_32
    #undef LEAF_COUNTS_TYPE_64

    string: {
        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        if constexpr (!expect_fixed) {
            LeafCounts fixed_leaf_counts;
            LeafCounts var_leafs_count;
            uint64_t min_byte_size, max_byte_size;
            uint16_t level_size_leafs;
            SIZE alignment;

            YYCURSOR = lex_range_argument(
                YYCURSOR,
                [&buffer, &fixed_leaf_counts, &var_leafs_count, &min_byte_size, &max_byte_size, &level_size_leafs, &alignment](uint32_t length) {
                    fixed_leaf_counts = {1, 0, 0, 0};
                    var_leafs_count = {0};
                    min_byte_size = max_byte_size = length;
                    level_size_leafs = 0;
                    alignment = SIZE::SIZE_1;
                    FixedStringType::create(buffer, length);
                },
                [&buffer, &fixed_leaf_counts, &var_leafs_count, &min_byte_size, &max_byte_size, &level_size_leafs, &alignment](Range range) {
                    auto [min, max] = range;
                    uint32_t delta = max - min;
                    SIZE size_size;
                    SIZE stored_size_size;
                    if (delta <= UINT8_MAX) {
                        fixed_leaf_counts = {1, 0, 0, 0};
                        stored_size_size = SIZE::SIZE_1;
                        min_byte_size = max_byte_size = 1;
                        if (max <= UINT8_MAX) {
                            size_size = SIZE::SIZE_1;
                        } else if (max <= UINT16_MAX) {
                            size_size = SIZE::SIZE_2;
                        } else {
                            size_size = SIZE::SIZE_4;
                        }
                    } else if (delta <= UINT16_MAX) {
                        fixed_leaf_counts = {0, 1, 0, 0};
                        stored_size_size = SIZE::SIZE_2;
                        min_byte_size = max_byte_size = 2;
                        if (max <= UINT16_MAX) {
                            size_size = SIZE::SIZE_2;
                        } else {
                            size_size = SIZE::SIZE_4;
                        }
                    } else /* if (delta <= UINT32_MAX) */ {
                        fixed_leaf_counts = {0, 0, 1, 0};
                        stored_size_size = SIZE::SIZE_4;
                        min_byte_size = max_byte_size = 4;
                        size_size = SIZE::SIZE_4;
                    }
                    min_byte_size += min;
                    max_byte_size += max;
                    var_leafs_count = {1, 0, 0, 0};
                    level_size_leafs = 1;
                    alignment = stored_size_size;
                    StringType::create(buffer, min, stored_size_size, size_size);
                }
            );
            return LexTypeResult{
                YYCURSOR,
                fixed_leaf_counts,
                var_leafs_count,
                {0},
                min_byte_size,
                max_byte_size,
                0,
                0,
                0,
                0,
                level_size_leafs,
                alignment
            };
        } else {
            uint64_t byte_size;
            YYCURSOR = lex_range_argument(
                YYCURSOR,
                [&buffer, &byte_size](uint32_t length) {
                    byte_size = length;
                    FixedStringType::create(buffer, length);
                },
                [identifier_start, &YYCURSOR](Range) {
                    show_syntax_error("expected fixed size string", identifier_start, YYCURSOR - 1);
                }
            );
            return LexFixedTypeResult{
                YYCURSOR,
                {1, 0, 0, 0},
                {0},
                byte_size,
                0,
                0,
                0,
                SIZE::SIZE_1
            };
        }
    }

    array: {
        auto [extended_idx, base_idx] = ArrayType::create(buffer);

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        YYCURSOR = skip_white_space(YYCURSOR);

        auto result = lex_type<true>(YYCURSOR, buffer, identifier_map);
        YYCURSOR = result.cursor;

        YYCURSOR = lex_same_line_symbol<',', "expected length argument">(YYCURSOR);

        auto extended = buffer.get(extended_idx);
        auto base = buffer.get(base_idx);

        if constexpr (!expect_fixed) {
            LeafCounts fixed_leaf_counts;
            LeafCounts var_leafs_count;
            uint64_t min_byte_size, max_byte_size;
            uint16_t level_size_leafs;
            SIZE alignment;

            YYCURSOR = lex_range_argument(
                YYCURSOR, 
                [&buffer, &extended, &base, &fixed_leaf_counts, &var_leafs_count, &min_byte_size, &max_byte_size, &level_size_leafs, &alignment, &result](uint32_t length) {
                    base->type = ARRAY_FIXED;
                    extended->length = length;
                    extended->stored_size_size = SIZE::SIZE_0;
                    extended->size_size = get_size_size(length);
                    fixed_leaf_counts = result.fixed_leaf_counts;
                    var_leafs_count = {0};
                    min_byte_size = max_byte_size = length * result.byte_size;
                    level_size_leafs = 0;
                    alignment = result.alignment;
                },
                [&buffer, &extended, &base, &fixed_leaf_counts, &var_leafs_count, &min_byte_size, &max_byte_size, &level_size_leafs, &alignment, &result](Range range) {
                    base->type = ARRAY;
                    auto [min, max] = range;
                    uint32_t delta = max - min;
                    SIZE size_size;
                    SIZE stored_size_size;
                    if (delta <= UINT8_MAX) {
                        fixed_leaf_counts = {1, 0, 0, 0};
                        stored_size_size = SIZE::SIZE_1;
                        alignment = result.alignment;
                        min_byte_size = max_byte_size = 1;
                        if (max <= UINT8_MAX) {
                            size_size = SIZE::SIZE_1;
                        } else if (max <= UINT16_MAX) {
                            size_size = SIZE::SIZE_2;
                        } else {
                            size_size = SIZE::SIZE_4;
                        }
                    } else if (delta <= UINT16_MAX) {
                        fixed_leaf_counts = {0, 1, 0, 0};
                        stored_size_size = SIZE::SIZE_2;
                        alignment = std::max(result.alignment, SIZE::SIZE_2);
                        min_byte_size = max_byte_size = 2;
                        if (max <= UINT16_MAX) {
                            size_size = SIZE::SIZE_2;
                        } else {
                            size_size = SIZE::SIZE_4;
                        }
                    } else /* if (delta <= UINT32_MAX) */ {
                        fixed_leaf_counts = {0, 0, 1, 0};
                        stored_size_size = SIZE::SIZE_4;
                        size_size = SIZE::SIZE_4;
                        alignment = std::max(result.alignment, SIZE::SIZE_4);
                        min_byte_size = max_byte_size = 4;
                    }
                    min_byte_size += result.byte_size * min;
                    max_byte_size += result.byte_size * max;
                    var_leafs_count = result.fixed_leaf_counts;
                    level_size_leafs = 1;
                    extended->length = min;
                    extended->stored_size_size = stored_size_size;
                    extended->size_size = size_size;
                }
            );
            return LexTypeResult{
                YYCURSOR,
                fixed_leaf_counts,
                var_leafs_count,
                result.variant_field_counts,
                min_byte_size,
                max_byte_size,
                result.total_variant_count,
                result.level_variant_fields,
                result.total_variant_fixed_leafs,
                0,
                level_size_leafs,
                alignment
            };
        } else {
            uint64_t byte_size;
            YYCURSOR = lex_range_argument(
                YYCURSOR, 
                [&buffer, &extended, &base, &byte_size, &result](uint32_t length) {
                    byte_size = length * result.byte_size;
                    base->type = ARRAY_FIXED;
                    extended->length = length;
                    extended->stored_size_size = SIZE::SIZE_0;
                    extended->size_size = get_size_size(length);
                },
                [identifier_start, YYCURSOR](Range) {
                    show_syntax_error("expected fixed size array", identifier_start, YYCURSOR - 1);
                }
            );
            return LexFixedTypeResult{
                YYCURSOR,
                result.fixed_leaf_counts,
                result.variant_field_counts,
                byte_size,
                result.total_variant_count,
                result.level_variant_fields,
                result.total_variant_fixed_leafs,
                result.alignment
            };
        }
    }

    variant: {

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        auto created_variant_type = DynamicVariantType::create(buffer);

        _VariantTypeMeta<!expect_fixed> type_meta_mem[8];
        Buffer type_meta_buffer = {type_meta_mem}; // Disposed in [add_variant_type]

        auto types_start_idx = buffer.position_idx<uint8_t>();

        return lex_variant_types<false, expect_fixed>(
            YYCURSOR,
            UINT64_MAX,
            0,
            buffer,
            type_meta_buffer,
            identifier_map,
            created_variant_type,
            types_start_idx,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            SIZE::SIZE_1
        );        
    }

    identifier: {
        auto identifer_end = YYCURSOR;

        auto identifier_idx_iter = identifier_map.find(std::string_view{identifier_start, identifer_end});
        if (identifier_idx_iter == identifier_map.end()) {
            show_syntax_error("identifier not defined", identifier_start, identifer_end - 1);
        }
        auto identifier_index = identifier_idx_iter->second;
        IdentifiedType::create(buffer, identifier_index);

        auto identifier = buffer.get(identifier_index);
        switch (identifier->keyword)
        {
        case UNION:
        case STRUCT: {
            auto struct_definition = identifier->data()->as_struct();
            if constexpr (!expect_fixed) {
                return LexTypeResult{
                    YYCURSOR,
                    struct_definition->fixed_leaf_counts,
                    struct_definition->var_leafs_count,
                    struct_definition->variant_field_counts,
                    struct_definition->min_byte_size,
                    struct_definition->max_byte_size,
                    struct_definition->total_variant_count,
                    struct_definition->level_variant_fields,
                    struct_definition->total_variant_fixed_leafs,
                    struct_definition->total_variant_var_leafs,
                    struct_definition->level_size_leafs,
                    struct_definition->max_alignment
                };
            } else {
                if (struct_definition->var_leafs_count.as_uint64 != 0) {
                    show_syntax_error("fixed size struct expected", identifier_start, YYCURSOR - 1);
                }
                return LexFixedTypeResult{
                    YYCURSOR,
                    struct_definition->fixed_leaf_counts,
                    struct_definition->variant_field_counts,
                    struct_definition->max_byte_size,
                    struct_definition->total_variant_count,
                    struct_definition->level_variant_fields,
                    struct_definition->total_variant_fixed_leafs,
                    struct_definition->max_alignment
                };
            }
            
        }
        case ENUM: {
            auto enum_definition = identifier->data()->as_enum();
            SIZE type_size = enum_definition->type_size;
            LeafCounts fixed_leaf_counts = {type_size};
            uint64_t byte_size = byte_size_of(type_size);
            if constexpr (!expect_fixed) {
                return LexTypeResult{
                    YYCURSOR,
                    fixed_leaf_counts,
                    {0},
                    {0},
                    byte_size,
                    byte_size,
                    0,
                    0,
                    0,
                    0,
                    0,
                    type_size
                };
            } else {
                return LexFixedTypeResult{
                    YYCURSOR,
                    fixed_leaf_counts,
                    {0},
                    byte_size,
                    0,
                    0,
                    0,
                    type_size
                };
            }
        }
        default:
            INTERNAL_ERROR("unreachable");
        }
    }
}

template <bool is_first_field>
INLINE char* lex_struct_or_union_fields (
    char* YYCURSOR,
    Buffer::Index<DefinitionWithFields> definition_data_idx,
    IdentifierMap &identifier_map,
    Buffer &buffer,
    LeafCounts fixed_leaf_counts,
    LeafCounts var_leafs_count,
    LeafCounts variant_field_counts,
    uint64_t min_byte_size,
    uint64_t max_byte_size,
    uint16_t level_size_leafs,
    uint16_t total_variant_count,
    uint16_t level_variant_fields,
    uint16_t total_variant_fixed_leafs,
    uint16_t total_variant_var_leafs,
    uint16_t field_count,
    SIZE max_alignment
) {
    before_field:
    YYCURSOR = skip_any_white_space(YYCURSOR);

    /*!local:re2c
        [a-zA-Z_]   { goto name_start; }
        "}"         { goto struct_end; }

        * { UNEXPECTED_INPUT("expected field name or '}'"); }
    */

    struct_end: {
        if constexpr (is_first_field) {
            show_syntax_error("expected at least one field", YYCURSOR - 1);
        } else {
            auto definition_data = buffer.get(definition_data_idx);
            definition_data->fixed_leaf_counts = fixed_leaf_counts;
            definition_data->var_leafs_count = var_leafs_count;
            definition_data->variant_field_counts = variant_field_counts;
            definition_data->min_byte_size = min_byte_size;
            definition_data->max_byte_size = max_byte_size;
            definition_data->level_size_leafs = level_size_leafs;
            definition_data->total_variant_count = total_variant_count;
            definition_data->level_variant_fields = level_variant_fields;
            definition_data->total_variant_fixed_leafs = total_variant_fixed_leafs;
            definition_data->total_variant_var_leafs = total_variant_var_leafs;
            definition_data->field_count = field_count;
            definition_data->max_alignment = max_alignment;
            return YYCURSOR;
        }
    }

    name_start:
    char* start = YYCURSOR - 1;
    /*!local:re2c
        [a-zA-Z0-9_]*  { goto name_end; }
    */
    name_end:
    char* end = YYCURSOR;
    size_t length = end - start;
    if constexpr (!is_first_field) {
        const StructField::Data* field = buffer.get(definition_data_idx)->first_field()->data();
        for (uint16_t i = 0; i < field_count; i++) {
            if (string_view_equal(field->name, start, length)) {
                show_syntax_error("field already defined", start, length);
            }
            field = skip_type<StructField>(field->type())->data();
        }
    }
    /*!local:re2c
        white_space* ":" { goto struct_field; }
        * { UNEXPECTED_INPUT("expected ':'"); }
    */

    struct_field: {
        StructField::Data* field = DefinitionWithFields::reserve_field(buffer);
        field->name = {start, length};
        
        YYCURSOR = skip_white_space(YYCURSOR);
        auto result = lex_type<false>(YYCURSOR, buffer, identifier_map);
        YYCURSOR = result.cursor;

        YYCURSOR = lex_same_line_symbol<';'>(YYCURSOR);

        if constexpr (is_first_field) {
            return lex_struct_or_union_fields<false>(
                YYCURSOR,
                definition_data_idx,
                identifier_map,
                buffer,
                result.fixed_leaf_counts,
                result.var_leafs_count,
                result.variant_field_counts,
                result.min_byte_size,
                result.max_byte_size,
                result.level_size_leafs,
                result.total_variant_count,
                result.level_variant_fields,
                result.total_variant_fixed_leafs,
                result.total_variant_var_leafs,
                1,
                result.alignment
            );
        } else {
            field_count++;
            fixed_leaf_counts += result.fixed_leaf_counts;
            var_leafs_count += result.var_leafs_count;
            min_byte_size += result.min_byte_size;
            max_byte_size += result.max_byte_size;
            level_size_leafs += result.level_size_leafs;
            total_variant_fixed_leafs += result.total_variant_fixed_leafs;
            total_variant_var_leafs += result.total_variant_var_leafs;
            total_variant_count += result.total_variant_count;
            level_variant_fields += result.level_variant_fields;
            variant_field_counts += result.variant_field_counts;
            max_alignment = std::max(max_alignment, result.alignment);
            goto before_field;
        }
    }
    
}

INLINE char* lex_struct_or_union(
    char* YYCURSOR,
    Buffer::Index<DefinitionWithFields> definition_data_idx,
    IdentifierMap &identifier_map,
    Buffer &buffer
) {
    YYCURSOR = lex_same_line_symbol<'{', "expected '{'">(YYCURSOR);

    return lex_struct_or_union_fields<true>(
        YYCURSOR,
        definition_data_idx,
        identifier_map,
        buffer,
        {0},
        {0},
        {0},
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        SIZE::SIZE_1
    );
}

INLINE auto set_member_value (char* start, uint64_t value, bool is_negative) {
    if (is_negative) {
        if (value == 1) {
            is_negative = false;
        } /*else if (value == 0) {
            INTERNAL_ERROR("[set_member_value] value would underflow");
        }*/ // this state should never happen since we dont call set_member_value with "-0"
        value--;
    } else {
        if (value == std::numeric_limits<uint64_t>::max()) {
            INTERNAL_ERROR("[set_member_value] value would overflow");
        }
        value++;
    }
    return std::pair(value, is_negative);
}

INLINE auto add_member (Buffer &buffer, char* start, char* end, uint64_t value, bool is_negative) {
    auto field = EnumDefinition::reserve_field(buffer);
    field->name = {start, end};
    field->value = value;
    field->is_negative = is_negative;
}

template <bool is_signed>
INLINE char* lex_enum_fields (
    char* YYCURSOR,
    Buffer::Index<EnumDefinition> definition_data_idx,
    uint16_t field_count,
    uint64_t value,
    uint64_t max_value_unsigned,
    bool is_negative,
    ankerl::unordered_dense::set<std::string_view>&& member_names,
    IdentifierMap& identifier_map,
    Buffer &buffer
) {
    while (1) {
        YYCURSOR = skip_any_white_space(YYCURSOR);

        /*!local:re2c
            [a-zA-Z_]   { goto name_start; }
            "}"         { goto enum_end; }

            * { UNEXPECTED_INPUT("expected field name or '}'"); }
        */

        enum_end: {
            if (field_count == 0) {
                show_syntax_error("expected at least one member", YYCURSOR - 1);
            }
            auto definition_data = buffer.get(definition_data_idx);
            definition_data->field_count = field_count;
            if (max_value_unsigned <= UINT8_MAX) {
                definition_data->type_size = SIZE::SIZE_1;
            } else if (max_value_unsigned <= UINT16_MAX) {
                definition_data->type_size = SIZE::SIZE_2;
            } else if (max_value_unsigned <= UINT32_MAX) {
                definition_data->type_size = SIZE::SIZE_4;
            } else {
                definition_data->type_size = SIZE::SIZE_8;
            }
            return YYCURSOR;
        }

        name_start:
        char* start = YYCURSOR - 1;
        /*!local:re2c
            [a-zA-Z0-9_]*  { goto name_end; }
        */
        name_end:
        char* end = YYCURSOR;
        size_t length = end - start;
        {   
            bool did_emplace = member_names.emplace(std::string_view{start, length}).second;
            if (!did_emplace) {
                show_syntax_error("field already defined", start, length);
            }
            //auto field = buffer.get(definition_data_idx)->first_field();
            //for (uint32_t i = 0; i < field_count; i++) {
            //    if (string_view_equal(field->name, start, length)) {
            //        show_syntax_error("field already defined", start, length);
            //    }
            //    field = field->next();
            //}
        }
        /*!local:re2c
            white_space* "," { goto default_value; }
            white_space* "=" { goto custom_value; }
            white_space* "}" { goto default_last_member; }
            * { UNEXPECTED_INPUT("expected custom value or ','"); }
        */

        default_value: {
            std::tie(value, is_negative) = set_member_value(start, value, is_negative);
            goto enum_member;
        }

        custom_value: {
            YYCURSOR = skip_white_space(YYCURSOR);
            is_negative = *YYCURSOR == '-';
            if constexpr (is_signed) {
                if (is_negative) {
                    auto parsed = parse_uint<uint64_t, std::numeric_limits<int64_t>::max()>(YYCURSOR + 1);
                    value = parsed.value;
                    YYCURSOR = parsed.cursor;
                    max_value_unsigned = std::max(max_value_unsigned, value * 2 - 1);

                    /*!local:re2c
                        white_space* "," { goto enum_member; }
                        white_space* "}" { goto last_member; }
                        * { UNEXPECTED_INPUT("expected ',' or end of enum definition"); }
                    */
                } else {
                    auto parsed = parse_uint<uint64_t, std::numeric_limits<int64_t>::max()>(YYCURSOR);
                    value = parsed.value;
                    YYCURSOR = parsed.cursor;
                    max_value_unsigned = std::max(max_value_unsigned, value * 2 + 1);

                    /*!local:re2c
                        white_space* "," { goto enum_member; }
                        white_space* "}" { goto last_member; }
                        * { UNEXPECTED_INPUT("expected ',' or end of enum definition"); }
                    */
                }
            } else {
                if (is_negative) {
                    auto parsed = parse_uint<uint64_t, std::numeric_limits<int64_t>::max()>(YYCURSOR + 1);
                    value = parsed.value;
                    YYCURSOR = parsed.cursor;
                    max_value_unsigned = std::max(max_value_unsigned, value * 2 - 1);

                    /*!local:re2c
                        white_space* "," { goto enum_member_signed; }
                        white_space* "}" { goto last_member; }
                        * { UNEXPECTED_INPUT("expected ',' or end of enum definition"); }
                    */
                    enum_member_signed: {
                        field_count++;
                        add_member(buffer, start, end, value, is_negative);
                        return lex_enum_fields<true>(YYCURSOR, definition_data_idx, field_count, value, max_value_unsigned, is_negative, std::move(member_names), identifier_map, buffer);
                    }
                } else {
                    auto parsed = parse_uint<uint64_t>(YYCURSOR);
                    value = parsed.value;
                    YYCURSOR = parsed.cursor;
                    max_value_unsigned = std::max(max_value_unsigned, value);

                    /*!local:re2c
                        white_space* "," { goto enum_member; }
                        white_space* "}" { goto last_member; }
                        * { UNEXPECTED_INPUT("expected ',' or end of enum definition"); }
                    */
                }
            }
        }

        default_last_member: {
            std::tie(value, is_negative) = set_member_value(start, value, is_negative);
            goto last_member;
        }

        last_member: {
            field_count++;
            add_member(buffer, start, end, value, is_negative);
            goto enum_end;
        }

        enum_member: {
            field_count++;
            add_member(buffer, start, end, value, is_negative);
        }
    }
}

INLINE char* lex_enum (
    char* YYCURSOR,
    Buffer::Index<EnumDefinition> definition_data_idx,
    IdentifierMap &identifier_map,
    Buffer &buffer
) {
    YYCURSOR = lex_same_line_symbol<'{', "expected '{'">(YYCURSOR);
    
    return lex_enum_fields<false>(YYCURSOR, definition_data_idx, 0, 1, 0, true, {}, identifier_map, buffer);
}


template <bool target_defined>
INLINE const StructDefinition* lex (char* YYCURSOR, IdentifierMap &identifier_map, Buffer &buffer, std::conditional_t<target_defined, const StructDefinition*, Empty> target) {
    loop: {
    /*!local:re2c
    
        struct_keyword  = any_white_space* "struct"  white_space;
        enum_keyword    = any_white_space* "enum"    white_space;
        union_keyword   = any_white_space* "union"   white_space;
        target_keyword  = any_white_space* "target"  white_space;

        any_white_space* [\x00] { goto eof; }

        * { UNEXPECTED_INPUT("unexpected input"); }

        struct_keyword  { goto struct_keyword; }
        enum_keyword    { goto enum_keyword; }
        union_keyword   { goto union_keyword; }
        target_keyword  { goto target_keyword; }

    */
    goto loop;
    }

    struct_keyword: {
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        auto [definition_data_idx, definition_idx] = StructDefinition::create(buffer);
        buffer.get(definition_idx)->keyword = KEYWORDS::STRUCT;
        buffer.get(definition_data_idx)->name = name_result.value;
        YYCURSOR = lex_struct_or_union(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(identifier_map, name_result.value, definition_idx);
        if constexpr (target_defined) {
            logger::warn("no possible path from target to struct ", name_result.value, " can be created.");
        }
        goto loop;
    }
    enum_keyword: {
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        auto [definition_data_idx, definition_idx] = EnumDefinition::create(buffer);
        buffer.get(definition_idx)->keyword = KEYWORDS::ENUM;
        buffer.get(definition_data_idx)->name = name_result.value;
        YYCURSOR = lex_enum(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(identifier_map, name_result.value, definition_idx);
        if constexpr (target_defined) {
            logger::warn("no possible path from target to enum ", name_result.value, " can be created.");
        }
        goto loop;
    }
    union_keyword: {
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        auto [definition_data_idx, definition_idx] = UnionDefinition::create(buffer);
        buffer.get(definition_idx)->keyword = KEYWORDS::UNION;
        buffer.get(definition_data_idx)->name = name_result.value;
        YYCURSOR = lex_struct_or_union(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(identifier_map, name_result.value, definition_idx);
        if constexpr (target_defined) {
            logger::warn("no possible path from target to union ", name_result.value, " can be created.");
        }
        goto loop;
    }
    target_keyword: {
        if constexpr (target_defined) {
            show_syntax_error("target already defined", YYCURSOR);
        } else {
            YYCURSOR = skip_white_space(YYCURSOR);
            auto type_idx = Buffer::Index<Type>{buffer.current_position()};
            YYCURSOR = lex_type<false>(YYCURSOR, buffer, identifier_map).cursor;
            YYCURSOR = lex_same_line_symbol<';'>(YYCURSOR);
            auto type = buffer.get(type_idx);
            if (type->type != FIELD_TYPE::IDENTIFIER) {
                INTERNAL_ERROR("target must be an identifier");
            }
            auto identifier_idx = type->as_identifier()->identifier_idx;
            auto identified_definition = buffer.get(identifier_idx);
            if (identified_definition->keyword != KEYWORDS::STRUCT) {
                INTERNAL_ERROR("target must be a struct");
            }
            return lex<true>(YYCURSOR, identifier_map, buffer, identified_definition->data()->as_struct());
        }
    }

    eof: {
        if constexpr (target_defined) {
            return target;
        } else {
            INTERNAL_ERROR("target not defined");
        }
    }
}

}