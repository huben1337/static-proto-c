#pragma once

#include "base.cpp"
#include "lexer_types.cpp"
#include "lex_result.cpp"
#include "lex_error.cpp"
#include "lex_helpers.re2c.cpp"
#include "parse_int.re2c.cpp"


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
    auto parsed_0 = parse_int<uint32_t>(YYCURSOR);
    YYCURSOR = parsed_0.cursor;
    auto min = parsed_0.value;

    YYCURSOR = skip_white_space(YYCURSOR);

    if (*YYCURSOR == '.') {
        YYCURSOR++;
        if (*YYCURSOR != '.') {
            UNEXPECTED_INPUT("expected '..' to mark range");
        }
        
        YYCURSOR = skip_white_space(YYCURSOR + 1);

        auto parsed_1 = parse_int<uint32_t>(YYCURSOR);
        YYCURSOR = parsed_1.cursor;
        auto max = parsed_1.value;
        if (max <= min) show_syntax_error("invalid range", firstArgPos, YYCURSOR - 1);
        on_range(Range{min, max});
    } else {
        on_fixed(min);
    }

    /*!local:re2c
        white_space* ">" { return YYCURSOR; }
        * { UNEXPECTED_INPUT("expected end of argument list"); }
    */
}

template <typename T>
requires (std::is_integral_v<T>)
INLINE LexResult<T> lex_attribute_value (char* YYCURSOR) {
    YYCURSOR = lex_same_line_symbol<'=', "Expected value assignment for attribute">(YYCURSOR);
    YYCURSOR = skip_white_space(YYCURSOR);
    return parse_int<uint64_t>(YYCURSOR);
}

INLINE LexResult<std::pair<char*, char*>>  lex_identifier_name (char* YYCURSOR) {
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

INLINE void add_identifier (std::pair<char*, char*> name, IdentifierMap &identifier_map, IdentifedDefinitionIndex definition_idx) {
    auto [start, end] = name;

    char end_backup = *end;
    *end = 0;
    auto inserted = identifier_map.insert({start, definition_idx}).second;
    *end = end_backup;

    if (!inserted) {
        show_syntax_error("identifier already defined", start);
    }
}

struct LexTypeResult {
    char* cursor;
    LeafCounts fixed_leaf_counts;
    LeafCounts var_leafs_count;
    LeafCounts variant_field_counts;
    uint64_t byte_size;
    uint16_t level_variant_fields;
    uint16_t total_variant_leafs;
    uint16_t size_leafs_count;
    SIZE alignment;
};

struct LexFixedTypeResult {
    char* cursor;
    LeafCounts fixed_leaf_counts;
    LeafCounts variant_field_counts;
    uint64_t byte_size;
    uint16_t level_variant_fields;
    uint16_t total_variant_leafs;
    SIZE alignment;
};


constexpr SIZE delta_to_size (uint32_t delta) {
    if (delta <= UINT8_MAX) {
        return SIZE_1;
    } else if (delta <= UINT16_MAX) {
        return SIZE_2;
    } else /* if (delta <= UINT32_MAX) */ {
        return SIZE_4;
    }
}

template <bool expect_fixed>
std::conditional_t<expect_fixed, LexFixedTypeResult, LexTypeResult> lex_type (char* YYCURSOR, Buffer &buffer, IdentifierMap &identifier_map) {
    const char* identifier_start = YYCURSOR;
    #define LEAF_COUNTS_TYPE_8  {1, 0, 0, 0}
    #define LEAF_COUNTS_TYPE_16 {0, 1, 0, 0}
    #define LEAF_COUNTS_TYPE_32 {0, 0, 1, 0}
    #define LEAF_COUNTS_TYPE_64 {0, 0, 0, 1}
    #define SIMPLE_TYPE(TYPE, ALIGN, fixed_leaf_counts)                                         \
    {                                                                                           \
        auto type = buffer.get_next<Type>();                                                    \
        type->type = FIELD_TYPE::TYPE;                                                          \
        if constexpr (expect_fixed) {                                                           \
            return LexFixedTypeResult{YYCURSOR, fixed_leaf_counts, {0}, ALIGN, 0, 0, ALIGN};    \
        } else {                                                                                \
            return LexTypeResult{YYCURSOR, fixed_leaf_counts, {0}, {0}, ALIGN, 0, 0, 0, ALIGN}; \
        }                                                                                       \
    }
    
    /*!local:re2c
        name = [a-zA-Z_][a-zA-Z0-9_]*;
                    
        "int8"      { SIMPLE_TYPE(INT8,    SIZE_1, LEAF_COUNTS_TYPE_8 ) }
        "int16"     { SIMPLE_TYPE(INT16,   SIZE_2, LEAF_COUNTS_TYPE_16) }
        "int32"     { SIMPLE_TYPE(INT32,   SIZE_4, LEAF_COUNTS_TYPE_32) }
        "int64"     { SIMPLE_TYPE(INT64,   SIZE_8, LEAF_COUNTS_TYPE_64) }
        "uint8"     { SIMPLE_TYPE(UINT8,   SIZE_1, LEAF_COUNTS_TYPE_8 ) }
        "uint16"    { SIMPLE_TYPE(UINT16,  SIZE_2, LEAF_COUNTS_TYPE_16) }
        "uint32"    { SIMPLE_TYPE(UINT32,  SIZE_4, LEAF_COUNTS_TYPE_32) }
        "uint64"    { SIMPLE_TYPE(UINT64,  SIZE_8, LEAF_COUNTS_TYPE_64) }
        "float32"   { SIMPLE_TYPE(FLOAT32, SIZE_4, LEAF_COUNTS_TYPE_32) }
        "float64"   { SIMPLE_TYPE(FLOAT64, SIZE_8, LEAF_COUNTS_TYPE_64) }
        "bool"      { SIMPLE_TYPE(BOOL,    SIZE_1, LEAF_COUNTS_TYPE_8 ) }
        "string"    { goto string;                                      }
        "array"     { goto array;                                       }
        "variant"   { goto variant;                                     }
        name        { goto identifier;                                  }

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
            uint64_t byte_size;
            uint16_t size_leafs_count;
            SIZE alignment;

            YYCURSOR = lex_range_argument(
                YYCURSOR,
                [&buffer, &fixed_leaf_counts, &var_leafs_count, &byte_size, &size_leafs_count, &alignment](uint32_t length) {
                    fixed_leaf_counts = {1, 0, 0, 0};
                    var_leafs_count = {0};
                    byte_size = length;
                    size_leafs_count = 0;
                    alignment = SIZE_1;
                    FixedStringType::create(buffer, length);
                },
                [&buffer, &fixed_leaf_counts, &var_leafs_count, &size_leafs_count, &alignment](Range range) {
                    auto [min, max] = range;
                    uint32_t delta = max - min;
                    SIZE size_size;
                    SIZE stored_size_size;
                    if (delta <= UINT8_MAX) {
                        fixed_leaf_counts = {1, 0, 0, 0};
                        stored_size_size = SIZE_1;
                        if (max <= UINT8_MAX) {
                            size_size = SIZE_1;
                        } else if (max <= UINT16_MAX) {
                            size_size = SIZE_2;
                        } else {
                            size_size = SIZE_4;
                        }
                    } else if (delta <= UINT16_MAX) {
                        fixed_leaf_counts = {0, 1, 0, 0};
                        stored_size_size = SIZE_2;
                        if (max <= UINT16_MAX) {
                            size_size = SIZE_2;
                        } else {
                            size_size = SIZE_4;
                        }
                    } else /* if (delta <= UINT32_MAX) */ {
                        fixed_leaf_counts = {0, 0, 1, 0};
                        stored_size_size = SIZE_4;
                        size_size = SIZE_4;
                    }
                    var_leafs_count = {1, 0, 0, 0};
                    size_leafs_count = 1;
                    alignment = stored_size_size;
                    StringType::create(buffer, range.min, stored_size_size, size_size);
                }
            );
            return LexTypeResult{YYCURSOR, fixed_leaf_counts, var_leafs_count, {0}, byte_size, 0, 0, size_leafs_count, alignment};
        } else {
            uint64_t byte_size;
            YYCURSOR = lex_range_argument(
                YYCURSOR,
                [&buffer, &byte_size](uint32_t length) {
                    byte_size = length;
                    FixedStringType::create(buffer, length);
                },
                [identifier_start](Range) {
                    show_syntax_error("expected fixed size string", identifier_start);
                }
            );
            return LexFixedTypeResult{YYCURSOR, {1, 0, 0, 0}, {0}, byte_size, 0, 0, SIZE_1};
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
            uint64_t byte_size;
            uint16_t size_leafs_count;
            SIZE alignment;

            YYCURSOR = lex_range_argument(
                YYCURSOR, 
                [&buffer, &extended, &base, &fixed_leaf_counts, &var_leafs_count, &byte_size, &size_leafs_count, &alignment, &result](uint32_t length) {
                    base->type = ARRAY_FIXED;
                    extended->length = length;
                    extended->stored_size_size = SIZE_0;
                    extended->size_size = SIZE_0;
                    fixed_leaf_counts = result.fixed_leaf_counts;
                    var_leafs_count = {0};
                    byte_size = length * result.byte_size;
                    size_leafs_count = 0;
                    alignment = result.alignment;
                },
                [&buffer, &extended, &base, &fixed_leaf_counts, &var_leafs_count, &size_leafs_count, &alignment, &result](Range range) {
                    base->type = ARRAY;
                    auto [min, max] = range;
                    uint32_t delta = max - min;
                    SIZE size_size;
                    SIZE stored_size_size;
                    if (delta <= UINT8_MAX) {
                        fixed_leaf_counts = {1, 0, 0, 0};
                        stored_size_size = SIZE_1;
                        alignment = result.alignment;
                        if (max <= UINT8_MAX) {
                            size_size = SIZE_1;
                        } else if (max <= UINT16_MAX) {
                            size_size = SIZE_2;
                        } else {
                            size_size = SIZE_4;
                        }
                    } else if (delta <= UINT16_MAX) {
                        fixed_leaf_counts = {0, 1, 0, 0};
                        stored_size_size = SIZE_2;
                        alignment = std::max(result.alignment, SIZE_2);
                        if (max <= UINT16_MAX) {
                            size_size = SIZE_2;
                        } else {
                            size_size = SIZE_4;
                        }
                    } else /* if (delta <= UINT32_MAX) */ {
                        fixed_leaf_counts = {0, 0, 1, 0};
                        stored_size_size = SIZE_4;
                        size_size = SIZE_4;
                        alignment = std::max(result.alignment, SIZE_4);
                    }
                    var_leafs_count = result.fixed_leaf_counts;
                    size_leafs_count = 1;
                    extended->length = min;
                    extended->stored_size_size = stored_size_size;
                    extended->size_size = size_size;
                }
            );
            return LexTypeResult{YYCURSOR, fixed_leaf_counts, var_leafs_count, result.variant_field_counts, byte_size, result.level_variant_fields, result.total_variant_leafs, size_leafs_count, alignment};
        } else {
            uint64_t byte_size;
            YYCURSOR = lex_range_argument(
                YYCURSOR, 
                [&buffer, &extended, &base, &byte_size, &result](uint32_t length) {
                    byte_size = length * result.byte_size;
                    base->type = ARRAY_FIXED;
                    extended->length = length;
                    extended->stored_size_size = SIZE_0;
                    extended->size_size = SIZE_0;
                },
                [identifier_start](Range) {
                    show_syntax_error("expected fixed size array", identifier_start);
                }
            );
            return LexFixedTypeResult{YYCURSOR, result.fixed_leaf_counts, result.variant_field_counts, byte_size, result.level_variant_fields, result.total_variant_leafs, result.alignment};
        }
    }

    variant: {

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        auto variant_type_idx = VariantType::create(buffer);
        variant_count_t variant_count = 0;

        uint64_t max_byte_size = 0;
        uint64_t min_byte_size = 0;
        // LeafCounts _mem[16];
        // auto leaf_counts_buffer = Buffer(_mem);

        uint16_t total_variant_leafs = 0;
        uint16_t level_variant_leafs = 0;
        uint16_t level_variant_fields = 0;

        VariantType::TypeMeta _leaf_counts_mem[4];
        auto leaf_counts_buffer = Buffer(_leaf_counts_mem);

        auto types_start_idx = buffer.position_idx<void>();

        SIZE max_alignment = SIZE_1;

        while (1) {
            variant_count++;
            YYCURSOR = skip_white_space(YYCURSOR);
            auto result = lex_type<true>(YYCURSOR, buffer, identifier_map);
            YYCURSOR = result.cursor;
            uint64_t byte_size = result.byte_size;
            if (byte_size > max_byte_size) {
                max_byte_size = byte_size;
            } else if (byte_size < min_byte_size) {
                min_byte_size = byte_size;
            }
            *leaf_counts_buffer.get_next<VariantType::TypeMeta>() = {result.fixed_leaf_counts, result.variant_field_counts};
            // *leaf_counts_buffer.get_next<LeafCounts>() = result.fixed_leaf_counts;
            uint16_t total_fixed_leafs = result.fixed_leaf_counts.total();
            total_variant_leafs += total_fixed_leafs + result.total_variant_leafs;
            level_variant_leafs += total_fixed_leafs;
            level_variant_fields += result.level_variant_fields;

            max_alignment = std::max(max_alignment, result.alignment);

            /*!local:re2c
                white_space* "," { continue; }
                white_space* ">" { break; }
                * { UNEXPECTED_INPUT("expected ',' or '>'"); }
            */
        }

        uint64_t max_wasted_bytes = 32;
        uint64_t shared_id;
        bool has_shared_id = false;
        bool has_max_wasted_bytes = false;

        char* YYMARKER = YYCURSOR;
        /*!local:re2c
            white_space* "[" { goto maybe_lex_attributes; }
            * { YYCURSOR = YYMARKER; goto max_wasted_bytes_check; }
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
                "]" { goto max_wasted_bytes_check; }
                * { show_syntax_error("expected closing of attributes", YYCURSOR); }
            */
        }
        max_wasted_bytes_check:
        if ((max_byte_size - min_byte_size) > max_wasted_bytes) {
            show_syntax_error("too many bytes wasted in variant", identifier_start);
        }
        LeafCounts fixed_leaf_counts;
        uint64_t byte_size;
        if (variant_count <= UINT8_MAX) {
            fixed_leaf_counts = {1, 0, 0, 0};
            byte_size = max_byte_size + 1;
        } else {
            fixed_leaf_counts = {0, 1, 0, 0};
            byte_size = max_byte_size + 2;
        }

        auto types_end_idx = buffer.position_idx<void>();

        uint32_t leaf_counts_mem_size = leaf_counts_buffer.current_position();
        if (leaf_counts_mem_size != variant_count * sizeof(VariantType::TypeMeta)) {
            printf("WATT");
            exit(1);
        }
        buffer.next_multi_byte<void>(variant_count * sizeof(VariantType::TypeMeta));

        auto types_start_ptr = reinterpret_cast<uint8_t*>(buffer.get(types_start_idx));
        auto types_end_ptr = reinterpret_cast<uint8_t*>(buffer.get(types_end_idx));
        // based on the way the memory overlaps we can reverse copy
        uint8_t* types_src = types_end_ptr - 1;
        uint8_t* types_dst = types_end_ptr - 1 + leaf_counts_mem_size;
        /* while (types_src >= types_start_ptr + sizeof(size_t)) {
            *(std::assume_aligned<alignof(size_t)>(reinterpret_cast<size_t*>(types_dst))) = *(std::assume_aligned<alignof(size_t)>(reinterpret_cast<const size_t*>(types_src)));
            types_src -= sizeof(size_t);
            types_dst -= sizeof(size_t);
        } */
        while (types_src >= types_start_ptr) {
            *(types_dst--) = *(types_src--);
        }
        
        // printf("aligned: %d", (size_t)types_start_ptr % 8); - use this to confirm alignment - now VariantType is 8 byte aligned so it should always be aligned
        auto leafs_count_src = leaf_counts_buffer.get<VariantType::TypeMeta>({0});
        auto leafs_count_dst = reinterpret_cast<VariantType::TypeMeta*>(types_start_ptr);
        for (size_t i = 0; i < variant_count; i++) {
            *leafs_count_dst = *leafs_count_src;
            leafs_count_dst++;
            leafs_count_src++;
        }
        // memcpy(types_start_ptr, leaf_counts_buffer.get<LeafCounts>({0}), leaf_counts_mem_size);
        
        auto variant_type = buffer.get(variant_type_idx);
        variant_type->variant_count = variant_count;
        variant_type->max_alignment = max_alignment;
        variant_type->level_variant_leafs = level_variant_leafs;

        LeafCounts variant_field_counts;
        switch (max_alignment)
        {
            case SIZE_1:
                variant_field_counts = {1, 0, 0, 0};
                break;
            case SIZE_2:
                variant_field_counts = {0, 1, 0, 0};
                break;
            case SIZE_4:
                variant_field_counts = {0, 0, 1, 0};
                break;
            case SIZE_8:
                variant_field_counts = {0, 0, 0, 1};
                break;
            default:
                variant_field_counts = {0};
        }
        

        if constexpr (!expect_fixed) {
            return LexTypeResult{YYCURSOR, fixed_leaf_counts, {0}, variant_field_counts, byte_size, 1, total_variant_leafs, 0, max_alignment};
        } else {
            return LexFixedTypeResult{YYCURSOR, fixed_leaf_counts, variant_field_counts, byte_size, 1, total_variant_leafs, max_alignment};
        }
    }

    identifier: {
        auto identifer_end = YYCURSOR;
        auto end_backup = *identifer_end;
        *identifer_end = 0;

        auto identifier_idx_iter = identifier_map.find(identifier_start);
        *identifer_end = end_backup;
        if (identifier_idx_iter == identifier_map.end()) {
            show_syntax_error("identifier not defined", identifier_start - 1);
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
                    struct_definition->byte_size,
                    struct_definition->level_variant_fields,
                    struct_definition->total_variant_leafs,
                    struct_definition->size_leafs_count,
                    struct_definition->max_alignment
                };
            } else {
                if (struct_definition->var_leafs_count.as_uint64 != 0) {
                    show_syntax_error("fixed size struct expected", identifier_start);
                }
                return LexFixedTypeResult{
                    YYCURSOR,
                    struct_definition->fixed_leaf_counts,
                    struct_definition->variant_field_counts,
                    struct_definition->byte_size,
                    struct_definition->level_variant_fields,
                    struct_definition->total_variant_leafs,
                    struct_definition->max_alignment
                };
            }
            
        }
        case ENUM: {
            auto enum_definition = identifier->data()->as_enum();
            LeafCounts fixed_leaf_counts = {1ULL << (enum_definition->type_size * sizeof(uint16_t))};
            uint64_t byte_size = 1ULL << enum_definition->type_size;
            if constexpr (!expect_fixed) {
                return {YYCURSOR, fixed_leaf_counts, {0}, {0}, byte_size, 0, 0, 0, static_cast<SIZE>(byte_size)};
            } else {
                return {YYCURSOR, fixed_leaf_counts, {0}, byte_size, 0, 0, static_cast<SIZE>(byte_size)};
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
    uint64_t byte_size,
    uint16_t size_leafs_count,
    uint16_t level_variant_fields,
    uint16_t total_variant_leafs,
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
            definition_data->byte_size = byte_size;
            definition_data->size_leafs_count = size_leafs_count;
            definition_data->level_variant_fields = level_variant_fields;
            definition_data->total_variant_leafs = total_variant_leafs;
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
    if (length > std::numeric_limits<uint16_t>::max()) {
        UNEXPECTED_INPUT("field name too long");
    }
    if constexpr (!is_first_field) {
        StructField::Data* field = buffer.get(definition_data_idx)->first_field()->data();
        for (uint32_t i = 0; i < field_count; i++) {
            if (string_section_eq(field->name.offset, field->name.length, start, length)) {
                show_syntax_error("field already defined", start);
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
        field->name = {start, static_cast<uint16_t>(length)};
        
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
                result.byte_size,
                result.size_leafs_count,
                result.level_variant_fields,
                result.total_variant_leafs,
                1,
                result.alignment
            );
        } else {
            field_count++;
            fixed_leaf_counts += result.fixed_leaf_counts;
            var_leafs_count += result.var_leafs_count;
            byte_size += result.byte_size;
            size_leafs_count += result.size_leafs_count;
            total_variant_leafs += result.total_variant_leafs;
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

    return lex_struct_or_union_fields<true>(YYCURSOR, definition_data_idx, identifier_map, buffer, {0}, {0}, {0}, 0, 0, 0, 0, 0, SIZE_1);
}

INLINE auto set_member_value (char* start, uint64_t value, bool is_negative) {
    if (value == std::numeric_limits<uint64_t>::max()) {
        show_syntax_error("enum member value too large", start);
    }
    if (is_negative) {
        if (value == 1) {
            is_negative = false;
        }
        value--;
    } else {
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
    IdentifierMap &identifier_map,
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
                definition_data->type_size = EnumDefinition::SIZE_1;
            } else if (max_value_unsigned <= UINT16_MAX) {
                definition_data->type_size = EnumDefinition::SIZE_2;
            } else if (max_value_unsigned <= UINT32_MAX) {
                definition_data->type_size = EnumDefinition::SIZE_4;
            } else {
                definition_data->type_size = EnumDefinition::SIZE_8;
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
            auto field = buffer.get(definition_data_idx)->first_field();
            for (uint32_t i = 0; i < field_count; i++) {
                if (string_section_eq(field->name.offset, field->name.length, start, length)) {
                    show_syntax_error("field already defined", start);
                }
                field = field->next();
            }
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
                    auto parsed = parse_int<uint64_t, std::numeric_limits<int64_t>::max()>(YYCURSOR + 1);
                    value = parsed.value;
                    YYCURSOR = parsed.cursor;
                    max_value_unsigned = std::max(max_value_unsigned, value * 2 - 1);

                    /*!local:re2c
                        white_space* "," { goto enum_member; }
                        white_space* "}" { goto last_member; }
                        * { UNEXPECTED_INPUT("expected ',' or end of enum definition"); }
                    */
                } else {
                    auto parsed = parse_int<uint64_t, std::numeric_limits<int64_t>::max()>(YYCURSOR);
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
                    auto parsed = parse_int<uint64_t, std::numeric_limits<int64_t>::max()>(YYCURSOR + 1);
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
                        return lex_enum_fields<true>(YYCURSOR, definition_data_idx, field_count, value, max_value_unsigned, is_negative, identifier_map, buffer);
                    }
                } else {
                    auto parsed = parse_int<uint64_t>(YYCURSOR);
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

    uint16_t field_count = 0;

    uint64_t value = 1;
    bool is_negative = true;

    uint64_t max_value_unsigned = 0;

    return lex_enum_fields<false>(YYCURSOR, definition_data_idx, field_count, value, max_value_unsigned, is_negative, identifier_map, buffer);
}

template <UnsignedIntegral T>
INLINE StringSection<T> to_identifier_string_section (std::pair<char*, char*> str) {
    auto [start, end] = str;
    size_t length = end - start;
    if (length > std::numeric_limits<T>::max()) {
        show_syntax_error("identifier name too long", start);
    }
    return {start, static_cast<T>(length)};
}

template <bool target_defined>
INLINE std::conditional_t<target_defined, void, StructDefinition*> lex (char* YYCURSOR, IdentifierMap &identifier_map, Buffer &buffer) {
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
        buffer.get(definition_data_idx)->name = to_identifier_string_section<uint16_t>(name_result.value);
        YYCURSOR = lex_struct_or_union(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(name_result.value, identifier_map, definition_idx);
        goto loop;
    }
    enum_keyword: {
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        auto [definition_data_idx, definition_idx] = EnumDefinition::create(buffer);
        buffer.get(definition_idx)->keyword = KEYWORDS::ENUM;
        buffer.get(definition_data_idx)->name = to_identifier_string_section<uint16_t>(name_result.value);
        YYCURSOR = lex_enum(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(name_result.value, identifier_map, definition_idx);
        goto loop;
    }
    union_keyword: {
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        auto [definition_data_idx, definition_idx] = UnionDefinition::create(buffer);
        buffer.get(definition_idx)->keyword = KEYWORDS::UNION;
        buffer.get(definition_data_idx)->name = to_identifier_string_section<uint16_t>(name_result.value);
        YYCURSOR = lex_struct_or_union(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(name_result.value, identifier_map, definition_idx);
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
            lex<true>(YYCURSOR, identifier_map, buffer);
            return buffer.get(identifier_idx)->data()->as_struct();
        }
    }

    eof: {
        if constexpr (!target_defined) {
            INTERNAL_ERROR("no target defined");
        }
    }
}

}