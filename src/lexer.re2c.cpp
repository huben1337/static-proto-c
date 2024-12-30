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
        white_space* "," { UNEXPECTED_INPUT("string only accepts one argument"); }
        * { UNEXPECTED_INPUT("expected end of argument list"); }
    */
}

INLINE LexResult<std::pair<char*, char*>>  lex_identifier_name (char* YYCURSOR) {
    /*!local:re2c
        white_space* [a-zA-Z_] { goto name_start; }

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
    LeafCounts leaf_counts;
    uint32_t internal_size;
    bool is_fixed_size;
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

LexTypeResult lex_type (char* YYCURSOR, Buffer &buffer, IdentifierMap &identifier_map) {
    const char* identifier_start = YYCURSOR;
    #define LEAF_COUNTS_TYPE_8  {1, 0, 0, 0}
    #define LEAF_COUNTS_TYPE_16 {0, 1, 0, 0}
    #define LEAF_COUNTS_TYPE_32 {0, 0, 1, 0}
    #define LEAF_COUNTS_TYPE_64 {0, 0, 0, 1}
    #define SIMPLE_TYPE(TYPE, ALIGN, LEAF_COUNTS)                       \
    {                                                                   \
        auto type = buffer.get_next<Type>();                            \
        type->type = FIELD_TYPE::TYPE;                                  \
        return {YYCURSOR, LEAF_COUNTS, sizeof_v<Type>, true};    \
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
        bool is_fixed_size;
        uint32_t internal_size;
        LeafCounts leaf_counts;

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        YYCURSOR = lex_range_argument(
            YYCURSOR,
            [&buffer, &is_fixed_size, &internal_size, &leaf_counts](uint32_t value) {
                is_fixed_size = true;
                internal_size = sizeof_v<Type> + alignof(FixedStringType) - 1 + sizeof_v<FixedStringType>;
                leaf_counts = {1, 0, 0, 0};
                FixedStringType::create(buffer, value);
            },
            [&buffer, &is_fixed_size, &internal_size, &leaf_counts](Range range) {
                is_fixed_size = false;
                auto [min, max] = range;
                uint32_t delta = max - min;
                SIZE size_size;
                SIZE stored_size_size;
                if (delta <= UINT8_MAX) {
                    leaf_counts = {1, 0, 0, 0};
                    stored_size_size = SIZE_1;
                    if (max <= UINT8_MAX) {
                        size_size = SIZE_1;
                    } else if (max <= UINT16_MAX) {
                        size_size = SIZE_2;
                    } else {
                        size_size = SIZE_4;
                    }
                } else if (delta <= UINT16_MAX) {
                    leaf_counts = {0, 1, 0, 0};
                    stored_size_size = SIZE_2;
                    if (max <= UINT16_MAX) {
                        size_size = SIZE_2;
                    } else {
                        size_size = SIZE_4;
                    }
                } else /* if (delta <= UINT32_MAX) */ {
                    leaf_counts = {0, 0, 1, 0};
                    stored_size_size = SIZE_4;
                    size_size = SIZE_4;
                }
                StringType::create(buffer, range.min, stored_size_size, size_size);
                internal_size = sizeof_v<Type> + alignof(StringType) - 1 + sizeof_v<StringType>; 
            }
        );

        
        return {YYCURSOR, leaf_counts, internal_size, is_fixed_size};
    }

    array: {
        auto [extended_idx, base_idx] = ArrayType::create(buffer);

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        YYCURSOR = skip_white_space(YYCURSOR);

        auto result = lex_type(YYCURSOR, buffer, identifier_map);
        if (!result.is_fixed_size) {
            show_syntax_error("expected static size type", YYCURSOR);
        }
        YYCURSOR = result.cursor;

        YYCURSOR = lex_same_line_symbol<',', "expected length argument">(YYCURSOR);

        bool is_fixed_size;
        LeafCounts leaf_counts;

        auto extended = buffer.get(extended_idx);
        auto base = buffer.get(base_idx);

        YYCURSOR = lex_range_argument(
            YYCURSOR, 
            [&buffer, &extended, &base, &is_fixed_size, &leaf_counts, &result](uint32_t length) {
                base->type = ARRAY_FIXED;
                extended->length = length;
                extended->stored_size_size = SIZE_0;
                extended->size_size = SIZE_0;
                leaf_counts = result.leaf_counts;
                is_fixed_size = true;
            },
            [&buffer, &extended, &base, &is_fixed_size, &leaf_counts](Range range) {
                base->type = ARRAY;
                auto [min, max] = range;
                uint32_t delta = max - min;
                SIZE size_size;
                SIZE stored_size_size;
                if (delta <= UINT8_MAX) {
                    leaf_counts = {1, 0, 0, 0};
                    stored_size_size = SIZE_1;
                    if (max <= UINT8_MAX) {
                        size_size = SIZE_1;
                    } else if (max <= UINT16_MAX) {
                        size_size = SIZE_2;
                    } else {
                        size_size = SIZE_4;
                    }
                } else if (delta <= UINT16_MAX) {
                    leaf_counts = {0, 1, 0, 0};
                    stored_size_size = SIZE_2;
                    if (max <= UINT16_MAX) {
                        size_size = SIZE_2;
                    } else {
                        size_size = SIZE_4;
                    }
                } else /* if (delta <= UINT32_MAX) */ {
                    leaf_counts = {0, 0, 1, 0};
                    stored_size_size = SIZE_4;
                    size_size = SIZE_4;
                }
                extended->length = min;
                extended->stored_size_size = stored_size_size;
                extended->size_size = size_size;
                is_fixed_size = false;
            }
        );

        return {YYCURSOR, leaf_counts, static_cast<uint32_t>(result.internal_size + sizeof_v<Type> + sizeof_v<ArrayType> + alignof(ArrayType) - 1), is_fixed_size};
    }

    variant: {

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        auto variant_type_idx = VariantType::create(buffer);
        variant_count_t variant_count = 0;


        uint32_t internal_size = sizeof_v<Type> + alignof(VariantType) - 1 + sizeof_v<VariantType>;
        LeafCounts leaf_counts = {0, 0, 0, 0};
        while (1) {
            variant_count++;
            YYCURSOR = skip_white_space(YYCURSOR);
            auto result = lex_type(YYCURSOR, buffer, identifier_map);
            YYCURSOR = result.cursor;
            internal_size += result.internal_size;
            leaf_counts += result.leaf_counts;
            /* auto variant_type_size = result.value;

            if (variant_count > 0) {
                type_size.alignment = std::max(variant_type_size.alignment, type_size.alignment);
                if (type_size.bit_size != variant_type_size.bit_size) {
                    type_size.bit_size = 0;
                }
            } else {
                type_size = {variant_type_size.bit_size, variant_type_size.alignment};
            } */

            /*!local:re2c
                white_space* "," { continue; }
                white_space* ">" { break; }
                * { UNEXPECTED_INPUT("expected ',' or '>'"); }
            */
            
        }

        buffer.get(variant_type_idx)->variant_count = variant_count;

        return {YYCURSOR, leaf_counts, internal_size, false};
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
            return {YYCURSOR, struct_definition->leaf_counts, struct_definition->internal_size, struct_definition->is_fixed_size};
        }
        case ENUM: {
            auto enum_definition = identifier->data()->as_enum();
            return {YYCURSOR, enum_definition->leaf_counts, enum_definition->internal_size, true};
        }
        default:
            INTERNAL_ERROR("unreachable");
        }
    }
}

template <typename T>
T* skip_type (Type* type);

template <typename T>
INLINE T* skip_variant_type (VariantType* variant_type) {
    auto types_count = variant_type->variant_count;
    Type* type = variant_type->first_variant();
    for (variant_count_t i = 0; i < types_count; i++) {
        type = skip_type<Type>(type);
    }
    return reinterpret_cast<T*>(type);
}

template <typename T>
T* skip_type (Type* type) {
    switch (type->type)
    {
    case STRING_FIXED:
        return reinterpret_cast<T*>(type->as_fixed_string() + 1);
    case STRING: {
        return reinterpret_cast<T*>(type->as_string() + 1);
    case ARRAY_FIXED:
    case ARRAY: {
        return skip_type<T>(type->as_array()->inner_type());
    }
    case VARIANT: {
        return skip_variant_type<T>(type->as_variant());
    }
    case IDENTIFIER:
        return reinterpret_cast<T*>(type->as_identifier() + 1);
    default:
        return reinterpret_cast<T*>(type + 1);
    }
    }
}

template <bool is_first_field>
INLINE char* lex_struct_or_union_fields (
    char* YYCURSOR,
    Buffer::Index<DefinitionWithFields> definition_data_idx,
    IdentifierMap &identifier_map,
    Buffer &buffer,
    LeafCounts leaf_counts,
    uint32_t internal_size,
    uint16_t field_count,
    bool is_fixed_size
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
            definition_data->leaf_counts = leaf_counts;
            definition_data->internal_size = internal_size;
            definition_data->field_count = field_count;
            definition_data->is_fixed_size = is_fixed_size;
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
        if constexpr (!is_first_field) {
            field_count++;
        }
        StructField::Data* field = DefinitionWithFields::reserve_field(buffer);
        field->name = {start, static_cast<uint16_t>(length)};
        
        YYCURSOR = skip_white_space(YYCURSOR);
        auto result = lex_type(YYCURSOR, buffer, identifier_map);
        YYCURSOR = result.cursor;
        internal_size += result.internal_size;
        leaf_counts += result.leaf_counts;

        YYCURSOR = lex_same_line_symbol<';'>(YYCURSOR);

        if constexpr (is_first_field) {
            return lex_struct_or_union_fields<false>(YYCURSOR, definition_data_idx, identifier_map, buffer, leaf_counts, internal_size, 1, result.is_fixed_size);
        } else {
            is_fixed_size &= result.is_fixed_size;
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

    return lex_struct_or_union_fields<true>(YYCURSOR, definition_data_idx, identifier_map, buffer, {0, 0, 0, 0}, sizeof_v<IdentifiedDefinition> + alignof(DefinitionWithFields) - 1 + sizeof_v<DefinitionWithFields>, 0, true);
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
            constexpr auto a = std::numeric_limits<int16_t>::min();
            if (max_value_unsigned <= UINT8_MAX) {
                definition_data->type_size = SIZE_1;
            } else if (max_value_unsigned <= UINT16_MAX) {
                definition_data->type_size = SIZE_2;
            } else if (max_value_unsigned <= UINT32_MAX) {
                definition_data->type_size = SIZE_4;
            } else {
                definition_data->type_size = SIZE_8;
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
INLINE StringSection<T> __to_string_section (std::pair<char*, char*> str) {
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
    
        struct_keyword  = any_white_space* "struct" ;
        enum_keyword    = any_white_space* "enum"   ;
        union_keyword   = any_white_space* "union"  ;
        typedef_keyword = any_white_space* "typedef";
        target_keyword  = any_white_space* "target" ;

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
        buffer.get(definition_data_idx)->name = __to_string_section<uint16_t>(name_result.value);
        YYCURSOR = lex_struct_or_union(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(name_result.value, identifier_map, definition_idx);
        goto loop;
    }
    enum_keyword: {
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        auto [definition_data_idx, definition_idx] = EnumDefinition::create(buffer);
        buffer.get(definition_idx)->keyword = KEYWORDS::ENUM;
        buffer.get(definition_data_idx)->name = __to_string_section<uint16_t>(name_result.value);
        YYCURSOR = lex_enum(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(name_result.value, identifier_map, definition_idx);
        goto loop;
    }
    union_keyword: {
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        auto [definition_data_idx, definition_idx] = UnionDefinition::create(buffer);
        buffer.get(definition_idx)->keyword = KEYWORDS::UNION;
        buffer.get(definition_data_idx)->name = __to_string_section<uint16_t>(name_result.value);
        YYCURSOR = lex_struct_or_union(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(name_result.value, identifier_map, definition_idx);
        goto loop;
    }
    /* typedef_keyword: {
        auto [definition_data, definition_idx] = TypedefDefinition::create(buffer);
        YYCURSOR = skip_white_space(YYCURSOR);
        YYCURSOR = lex_type(YYCURSOR, buffer, definition_data, identifier_map);
        YYCURSOR = lex_white_space(YYCURSOR);
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        YYCURSOR = lex_same_line_symbol<';', "expected ';'">(YYCURSOR);
        add_identifier(name_result.value, identifier_map, definition_data, definition_idx);
        goto loop;
    } */
    target_keyword: {
        if constexpr (target_defined) {
            show_syntax_error("target already defined", YYCURSOR);
        } else {
            YYCURSOR = skip_white_space(YYCURSOR);
            auto type_idx = Buffer::Index<Type>{buffer.current_position()};
            YYCURSOR = lex_type(YYCURSOR, buffer, identifier_map).cursor;
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