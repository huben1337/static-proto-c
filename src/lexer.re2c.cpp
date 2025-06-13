#pragma once

#include "base.cpp"
#include "fatal_error.cpp"
#include "lexer_types.cpp"
#include "lex_result.cpp"
#include "lex_error.cpp"
#include "lex_helpers.re2c.cpp"
#include "parse_int.re2c.cpp"
#include "string_helpers.cpp"
#include <concepts>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include "logger.cpp"

namespace lexer {

/*!re2c
    re2c:yyfill:enable = 0;
    re2c:define:YYCTYPE = char;

    re2c:api = generic;
    re2c:api:style = free-form;

    re2c:define:YYBACKUP     = "// YYBACKUP";
    re2c:define:YYBACKUPCTX  = "YYBACKUPCTX";
    re2c:define:YYLESSTHAN   = "YYLESSTHAN";
    re2c:define:YYMTAGN      = "YYMTAGN";
    re2c:define:YYMTAGP      = "YYMTAGP";
    re2c:define:YYPEEK       = "*YYCURSOR";
    re2c:define:YYRESTORE    = "// YYRESTORE";
    re2c:define:YYRESTORECTX = "YYRESTORECTX";
    re2c:define:YYRESTORETAG = "YYRESTORETAG";
    re2c:define:YYSKIP       = "++YYCURSOR;";
    re2c:define:YYSHIFT      = "YYSHIFT";
    re2c:define:YYCOPYMTAG   = "YYCOPYMTAG";
    re2c:define:YYCOPYSTAG   = "@@{lhs} = @@{rhs};";
    re2c:define:YYSHIFTMTAG  = "YYSHIFTMTAG";
    re2c:define:YYSHIFTSTAG  = "@@{tag} += @@{shift};// YYSHIFTSTAG";
    re2c:define:YYSTAGN      = "YYSTAGN";
    re2c:define:YYSTAGP      = "@@{tag} = YYCURSOR;";

    any_white_space = [ \t\r\n];
    white_space = [ \t];
*/

template <typename F, typename G>
INLINE char* lex_range_argument (char* YYCURSOR, F on_fixed, G on_range) {

    auto parsed_0 = parse_uint_skip_white_space<uint32_t, true>(YYCURSOR);
    YYCURSOR = parsed_0.cursor;
    const char* const range_start = YYCURSOR - parsed_0.digits;
    auto min = parsed_0.value;

    char* const after_min_cursor = YYCURSOR;

    /*!local:re2c
        any_white_space* "." { goto maybe_range; }
        any_white_space* { goto fixed; }
    */

    maybe_range: {
        if (*YYCURSOR == '.') {
            YYCURSOR++;
            goto range;
        } else {
            UNEXPECTED_INPUT("expected '..' to mark range");
        }
    }

    range: {
        auto parsed_1 = parse_uint_skip_white_space<uint32_t>(YYCURSOR);
        YYCURSOR = parsed_1.cursor;
        auto max = parsed_1.value;
        if (max <= min) [[unlikely]] {
            show_syntax_error("invalid range", range_start, YYCURSOR - 1);
        }
        on_range(Range{min, max});
        goto lex_argument_list_end;
    }

    fixed: {
        on_fixed(min);
        YYCURSOR = after_min_cursor;
    }

    lex_argument_list_end:
    /*!local:re2c
        any_white_space* ">"    { goto done; }
        any_white_space*        { UNEXPECTED_INPUT("expected end of argument list"); }
    */
    done:
    return YYCURSOR;
}

template <std::unsigned_integral T>
INLINE ParseNumberResult<T> lex_attribute_value (char* YYCURSOR) {
    YYCURSOR = lex_symbol<'=', "Expected value assignment for attribute">(YYCURSOR);
    return parse_uint_skip_white_space<T>(YYCURSOR);
}

struct VariantAttributes {
    uint64_t max_wasted_bytes;
    bool has_shared_id;
    uint32_t shared_id;
};

INLINE LexResult<VariantAttributes> lex_variant_attributes (char* YYCURSOR) {
    static constexpr uint64_t max_wasted_bytes_default = 32;


    /*!local:re2c
        white_space* "["    { goto maybe_lex_attributes; }
        white_space*        { goto no_attributes; }
    */

    no_attributes: {
        return {YYCURSOR, {max_wasted_bytes_default, false}};
    }

    maybe_lex_attributes: {
        uint64_t max_wasted_bytes = max_wasted_bytes_default;
        bool has_max_wasted_bytes = false;
        uint32_t shared_id;
        bool has_shared_id = false;

        /*!local:re2c
            "[" { goto lex_attributes; }
            * { show_syntax_error("expected second '[' to open attribute list", YYCURSOR - 1); }
        */
        
        lex_attributes: {
            /*!local:re2c
                white_space* "max_wasted"   { goto lex_max_wasted_bytes; }
                white_space* "shared_id"    { goto lex_shared_id; }
                white_space*                { show_syntax_error("expected attribute", YYCURSOR - 1); }
            */
            lex_max_wasted_bytes: {
                if (has_max_wasted_bytes) {
                    show_syntax_error("conflicting attributes", YYCURSOR - 1);
                }
                has_max_wasted_bytes = true;
                auto parsed = lex_attribute_value<uint32_t>(YYCURSOR);
                max_wasted_bytes = parsed.value;
                YYCURSOR = parsed.cursor;
                goto attribute_end;
            }
            lex_shared_id: {
                if (has_shared_id) {
                    show_syntax_error("conflicting attributes", YYCURSOR - 1);
                }
                has_shared_id = true;
                auto parsed = lex_attribute_value<uint32_t>(YYCURSOR);
                shared_id = parsed.value;
                YYCURSOR = parsed.cursor;
                goto attribute_end;
            }
            attribute_end: {
                /*!local:re2c
                    white_space* "]" { goto close_attributes; }
                    white_space* "," { goto lex_attributes; }
                    white_space* { show_syntax_error("expected end of attributes or next ", YYCURSOR); }
                */
            }
        }

        close_attributes:
        /*!local:re2c
            "]" { goto lex_attributes_done; }
            * { show_syntax_error("expected closing of attributes", YYCURSOR); }
        */
        lex_attributes_done:
        return {YYCURSOR, {max_wasted_bytes, has_shared_id, shared_id}};
    }
    
}


INLINE LexResult<std::string_view> lex_identifier_name (char* YYCURSOR) {
    const char* start;
    
    /*!stags:re2c format = 'const char *@@;\n'; */
    /*!local:re2c
        re2c:tags = 1;
        identifier = [a-zA-Z_][a-zA-Z0-9_]*;

        invalid = [^a-zA-Z0-9_];
        
        any_white_space* @start identifier      { goto done; }

        any_white_space* "int8"    invalid      { UNEXPECTED_INPUT("int8 is reserved");    }
        any_white_space* "int16"   invalid      { UNEXPECTED_INPUT("int16 is reserved");   }
        any_white_space* "int32"   invalid      { UNEXPECTED_INPUT("int32 is reserved");   }
        any_white_space* "int64"   invalid      { UNEXPECTED_INPUT("int64 is reserved");   }
        any_white_space* "uint8"   invalid      { UNEXPECTED_INPUT("uint8 is reserved");   }
        any_white_space* "uint16"  invalid      { UNEXPECTED_INPUT("uint16 is reserved");  }
        any_white_space* "uint32"  invalid      { UNEXPECTED_INPUT("uint32 is reserved");  }
        any_white_space* "uint64"  invalid      { UNEXPECTED_INPUT("uint64 is reserved");  }
        any_white_space* "float32" invalid      { UNEXPECTED_INPUT("float32 is reserved"); }
        any_white_space* "float64" invalid      { UNEXPECTED_INPUT("float64 is reserved"); }
        any_white_space* "bool"    invalid      { UNEXPECTED_INPUT("bool is reserved");    }
        any_white_space* "string"  invalid      { UNEXPECTED_INPUT("string is reserved");  }
        any_white_space* "array"   invalid      { UNEXPECTED_INPUT("array is reserved");   }
        any_white_space* "variant" invalid      { UNEXPECTED_INPUT("variant is reserved"); }
        any_white_space* "struct"  invalid      { UNEXPECTED_INPUT("struct is reserved");  }
        any_white_space* "enum"    invalid      { UNEXPECTED_INPUT("enum is reserved");    }
        any_white_space* "union"   invalid      { UNEXPECTED_INPUT("union is reserved");   }
        any_white_space* "target"  invalid      { UNEXPECTED_INPUT("target is reserved");  }

        any_white_space* { UNEXPECTED_INPUT("expected name"); }
    */
    done:
    return { YYCURSOR, {start, YYCURSOR} };
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
    auto [max_wasted_bytes, has_shared_id, shared_id] = attribute_lex_result.value;

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
                    any_white_space* "," { goto dynamic_variant_next; }
                    any_white_space* ">" { goto dynamic_variant_done;  }
                    any_white_space* { UNEXPECTED_INPUT("expected ',' or '>'"); }
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
            any_white_space* "," { continue; }
            any_white_space* ">" { goto variant_done; }
            any_white_space* { UNEXPECTED_INPUT("expected ',' or '>'"); }
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
    
    #define SIMPLE_TYPE(TYPE, ALIGN)                                                                                    \
    {                                                                                                                   \
        auto type = buffer.get_next<Type>();                                                                            \
        type->type = FIELD_TYPE::TYPE;                                                                                  \
        constexpr LeafCounts fixed_leaf_counts = {ALIGN};                                                               \
        constexpr uint64_t byte_size = ALIGN.byte_size();                                                               \
        if constexpr (expect_fixed) {                                                                                   \
            return LexFixedTypeResult{YYCURSOR, fixed_leaf_counts, {0}, byte_size, 0, 0, 0, ALIGN};                     \
        } else {                                                                                                        \
            return LexTypeResult{YYCURSOR, fixed_leaf_counts, {0}, {0}, byte_size, byte_size, 0, 0, 0, 0, 0, ALIGN};    \
        }                                                                                                               \
    }

    const char* typename_start; // Only initialized for non-simple types
    
    /*!stags:re2c format = 'const char *@@;\n'; */
    /*!local:re2c
        re2c:tags = 1;
        identifier = [a-zA-Z_][a-zA-Z0-9_]*;
                    
        any_white_space* "int8"                             { SIMPLE_TYPE(INT8,    SIZE::SIZE_1 ) }
        any_white_space* "int16"                            { SIMPLE_TYPE(INT16,   SIZE::SIZE_2 ) }
        any_white_space* "int32"                            { SIMPLE_TYPE(INT32,   SIZE::SIZE_4 ) }
        any_white_space* "int64"                            { SIMPLE_TYPE(INT64,   SIZE::SIZE_8 ) }
        any_white_space* "uint8"                            { SIMPLE_TYPE(UINT8,   SIZE::SIZE_1 ) }
        any_white_space* "uint16"                           { SIMPLE_TYPE(UINT16,  SIZE::SIZE_2 ) }
        any_white_space* "uint32"                           { SIMPLE_TYPE(UINT32,  SIZE::SIZE_4 ) }
        any_white_space* "uint64"                           { SIMPLE_TYPE(UINT64,  SIZE::SIZE_8 ) }
        any_white_space* "float32"                          { SIMPLE_TYPE(FLOAT32, SIZE::SIZE_4 ) }
        any_white_space* "float64"                          { SIMPLE_TYPE(FLOAT64, SIZE::SIZE_8 ) }
        any_white_space* "bool"                             { SIMPLE_TYPE(BOOL,    SIZE::SIZE_1 ) }
        any_white_space*  @typename_start "string"          { goto string;                        }
        any_white_space*  @typename_start "array"           { goto array;                         }
        any_white_space*  @typename_start "variant"         { goto variant;                       }
        any_white_space*  @typename_start identifier        { goto identifier;                    }

        any_white_space* { UNEXPECTED_INPUT("expected type"); }
    */
    #undef SIMPLE_TYPE
    #undef LEAF_COUNTS_TYPE_8
    #undef LEAF_COUNTS_TYPE_16
    #undef LEAF_COUNTS_TYPE_32
    #undef LEAF_COUNTS_TYPE_64

    string: {
        YYCURSOR = lex_symbol<'<', "expected argument list">(YYCURSOR);

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
                [typename_start, &YYCURSOR](Range) {
                    show_syntax_error("expected fixed size string", typename_start, YYCURSOR - 1);
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

        YYCURSOR = lex_symbol<'<', "expected argument list">(YYCURSOR);

        auto result = lex_type<true>(YYCURSOR, buffer, identifier_map);
        YYCURSOR = result.cursor;

        YYCURSOR = lex_symbol<',', "expected length argument">(YYCURSOR);

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
                [typename_start, YYCURSOR](Range) {
                    show_syntax_error("expected fixed size array", typename_start, YYCURSOR - 1);
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

        YYCURSOR = lex_symbol<'<', "expected argument list">(YYCURSOR);

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
        char* const typename_end = YYCURSOR;

        auto identifier_idx_iter = identifier_map.find(std::string_view{typename_start, typename_end});
        if (identifier_idx_iter == identifier_map.end()) {
            show_syntax_error("identifier not defined", typename_start, typename_end - 1);
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
                    show_syntax_error("fixed size struct expected", typename_start, YYCURSOR - 1);
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
            uint64_t byte_size = type_size.byte_size();
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
    /*!local:re2c
        any_white_space* [a-zA-Z_]   { goto name_start; }
        any_white_space* "}"         { goto struct_end; }

        any_white_space* { show_syntax_error("Expected field name or end of struct", YYCURSOR - 1); }
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
        any_white_space* ":" { goto struct_field; }
        any_white_space* { UNEXPECTED_INPUT("expected ':'"); }
    */

    struct_field: {
        StructField::Data* field = DefinitionWithFields::reserve_field(buffer);
        field->name = {start, length};
        
        auto result = lex_type<false>(YYCURSOR, buffer, identifier_map);
        YYCURSOR = result.cursor;

        YYCURSOR = lex_symbol<';'>(YYCURSOR);

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
    YYCURSOR = lex_symbol<'{', "Expected '{' to denote start of struct">(YYCURSOR);

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



template <bool is_signed>
INLINE char* lex_enum_fields (
    char* YYCURSOR,
    Buffer::Index<EnumDefinition> definition_data_idx,
    uint16_t field_count,
    EnumField::Value&& value,
    uint64_t max_value_unsigned,
    ankerl::unordered_dense::set<std::string_view>&& member_names,
    IdentifierMap& identifier_map,
    Buffer &buffer
) {
    while (1) {
        /*!local:re2c
            any_white_space* [a-zA-Z_]   { goto name_start; }
            any_white_space* "}"         { goto enum_end; }

            any_white_space* { show_syntax_error("Expected field name or end of struct", YYCURSOR - 1); }
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
        std::string_view name = {std::move(start), YYCURSOR};
        {   
            bool did_emplace = member_names.emplace(name).second;
            if (!did_emplace) {
                show_syntax_error("field already defined", name.data(), name.size());
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
            any_white_space* "," { goto default_value; }
            any_white_space* "=" { goto custom_value; }
            any_white_space* "}" { goto default_last_member; }
            any_white_space* { UNEXPECTED_INPUT("expected custom value or ','"); }
        */

        default_value: {
            value.increment();
            goto enum_member;
        }

        custom_value: {
            /*!local:re2c
                any_white_space* "-"    { goto parse_negative_value; }
                any_white_space*        { goto parse_positive_value; }
            */

            parse_negative_value: {
                auto parsed = parse_uint<uint64_t, false, static_cast<uint64_t>(std::numeric_limits<int64_t>::min())>(YYCURSOR);
                YYCURSOR = parsed.cursor;

                
                max_value_unsigned = std::max(max_value_unsigned, parsed.value * 2 - 1);

                bool is_negative = parsed.value != 0;
                value = {parsed.value, is_negative};

                /*!local:re2c
                    any_white_space* "," { goto enum_member_signed; }
                    any_white_space* "}" { goto last_member; }
                    any_white_space* { UNEXPECTED_INPUT("expected ',' or end of enum definition"); }
                */
                enum_member_signed: {
                    field_count++;
                    EnumDefinition::add_field(buffer, {name, value});
                    return lex_enum_fields<true>(YYCURSOR, definition_data_idx, field_count, std::move(value), max_value_unsigned, std::move(member_names), identifier_map, buffer);
                }
            }

            parse_positive_value: {
                auto parsed = parse_uint<
                    uint64_t,
                    false,
                    std::numeric_limits<std::conditional_t<
                        is_signed,
                        int64_t,
                        uint64_t
                    >>::max()
                >(YYCURSOR);
                YYCURSOR = parsed.cursor;
                if constexpr (is_signed) {
                    max_value_unsigned = std::max(max_value_unsigned, parsed.value * 2 - 1);
                } else {
                    max_value_unsigned = std::max(max_value_unsigned, parsed.value);
                }

                value = EnumField::Value{parsed.value, false};

                /*!local:re2c
                    any_white_space* "," { goto enum_member; }
                    any_white_space* "}" { goto last_member; }
                    any_white_space* { UNEXPECTED_INPUT("expected ',' or end of enum definition"); }
                */
            }
        }

        default_last_member: {
            value.increment();
            goto last_member;
        }

        last_member: {
            field_count++;
            EnumDefinition::add_field(buffer, {name, value});
            goto enum_end;
        }

        enum_member: {
            field_count++;
            EnumDefinition::add_field(buffer, {name, value});
        }
    }
}

INLINE char* lex_enum (
    char* YYCURSOR,
    Buffer::Index<EnumDefinition> definition_data_idx,
    IdentifierMap &identifier_map,
    Buffer &buffer
) {
    YYCURSOR = lex_symbol<'{', "Expected '{' to denote start of enum">(YYCURSOR);
    return lex_enum_fields<false>(YYCURSOR, definition_data_idx, 0, {-1LL}, 0, {}, identifier_map, buffer);
}


template <bool target_defined>
INLINE const StructDefinition* lex (char* YYCURSOR, IdentifierMap &identifier_map, Buffer &buffer, std::conditional_t<target_defined, const StructDefinition*, Empty> target) {
    loop: {
    /*!local:re2c

        any_white_space* "struct"  any_white_space  { goto struct_keyword; }
        any_white_space* "enum"    any_white_space  { goto enum_keyword; }
        any_white_space* "union"   any_white_space  { goto union_keyword; }
        any_white_space* "target"  any_white_space  { goto target_keyword; }

        any_white_space* [\x00]                     { goto eof; }

        any_white_space*                            { UNEXPECTED_INPUT("unexpected input"); }

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
            auto type_idx = Buffer::Index<Type>{buffer.current_position()};
            YYCURSOR = lex_type<false>(YYCURSOR, buffer, identifier_map).cursor;
            YYCURSOR = lex_symbol<';'>(YYCURSOR);
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