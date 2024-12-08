#pragma once
#include <cstdint>
#include <utility>
#include <unordered_map>
#include <functional>
#include "string_helpers.cpp"
#include "base.cpp"
#include "memory.cpp"
#include "memory_helpers.cpp"
#include "lex_result.cpp"
#include "lex_error.cpp"
#include "lex_helpers.re2c.cpp"
#include "parse_int.re2c.cpp"
#include "helper_types.cpp"

namespace lexer {

/*!re2c
    re2c:define:YYMARKER = YYCURSOR;
    re2c:yyfill:enable = 0;
    re2c:define:YYCTYPE = char;

    any_white_space = [ \t\r\n];
    white_space = [ \t];
*/

enum KEYWORDS : uint8_t {
    STRUCT,
    ENUM,
    UNION,
    // TYPEDEF,
};

enum FIELD_TYPE : uint8_t {
    BOOL,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    INT8,
    INT16,
    INT32,
    INT64,
    FLOAT32,
    FLOAT64,
    STRING_FIXED,
    STRING,
    ARRAY_FIXED,
    ARRAY,
    VARIANT,
    IDENTIFIER
};

enum SIZE : uint8_t {
    SIZE_0 = 0,
    SIZE_1 = 1,
    SIZE_2 = 2,
    SIZE_4 = 4,
    SIZE_8 = 8,
};

struct Range {
    uint32_t min;
    uint32_t max;
};

typedef uint16_t variant_count_t;

struct LeafCounts {
    uint16_t size8;
    uint16_t size16;
    uint16_t size32;
    uint16_t size64;

    void operator += (const LeafCounts& other) {
        size8 += other.size8;
        size16 += other.size16;
        size32 += other.size32;
        size64 += other.size64;
    }
};

struct IdentifiedDefinition {
    KEYWORDS keyword;

    struct Data {
        StringSection<uint16_t> name;
        LeafCounts leaf_counts;
        uint32_t internal_size;

        INLINE constexpr auto as_struct ();
        INLINE constexpr auto as_enum ();
        INLINE constexpr auto as_union ();
        // INLINE constexpr auto as_typedef ();
    };

    Data* data ();

};


struct Type {
    FIELD_TYPE type;

    INLINE auto as_fixed_string () const;
    INLINE auto as_string () const;
    INLINE auto as_identifier () const;
    INLINE auto as_array () const;
    INLINE auto as_variant () const;
};

struct TypeContainer {};


template <typename T, FIELD_TYPE type>
INLINE T* create_extended_type (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position() + sizeof_v<Type>);
    Buffer::Index<Type> type_idx = buffer.get_next_multi_byte<Type>(sizeof_v<Type> + padding + sizeof_v<T>);
    Type* __type = buffer.get(type_idx);
    __type->type = type;
    auto extended_idx = Buffer::Index<T>{static_cast<uint32_t>(type_idx.value + padding + sizeof_v<Type>)};
    return buffer.get_aligned(extended_idx);
}

template <typename T>
INLINE T* get_extended_type(auto* that) {
    return get_extended<T, Type>(that);
}


struct FixedStringType {
    INLINE static void create (Buffer &buffer, uint32_t length) {
        auto [extended, base] = create_extended<FixedStringType, Type>(buffer);
        base->type = FIELD_TYPE::STRING_FIXED;
        extended->length = length;
    }

    uint32_t length;
};
INLINE auto Type::as_fixed_string () const {
    return get_extended_type<FixedStringType>(this);
}

struct StringType {
    template <SIZE alignment>
    requires (alignment != SIZE_0)
    INLINE static void create (Buffer &buffer, uint32_t min_length) {
        auto [extended, base] = create_extended<StringType, Type>(buffer);
        base->type = STRING;
        extended->min_length = min_length;
        extended->fixed_alignment = alignment;
    }
    uint32_t min_length;
    SIZE fixed_alignment;
};
INLINE auto Type::as_string () const {
    return get_extended_type<StringType>(this);
}


typedef Buffer::Index<IdentifiedDefinition> IdentifedDefinitionIndex;
struct IdentifiedType {
    INLINE static void create (Buffer &buffer, IdentifedDefinitionIndex identifier_idx) {
        auto type = create_extended_type<IdentifiedType, IDENTIFIER>(buffer);
        type->identifier_idx = identifier_idx;
    }

    IdentifedDefinitionIndex identifier_idx;
};
INLINE auto Type::as_identifier () const {
    return get_extended_type<IdentifiedType>(this);
}

struct ArrayType : TypeContainer {

    INLINE static auto create (Buffer &buffer) {
        return create_extended<ArrayType, Type>(buffer);
    }

    uint32_t length;
    SIZE fixed_alignment;

    INLINE Type* inner_type () {
        return reinterpret_cast<Type*>(this + 1);
    }
    
};
INLINE auto Type::as_array () const {
    return get_extended_type<ArrayType>(this);
}

struct VariantType : TypeContainer {
    INLINE static VariantType* create (Buffer &buffer) {
        return create_extended_type<VariantType, VARIANT>(buffer);
    }

    variant_count_t variant_count;

    INLINE Type* first_variant() {
        return (Type*)(this + 1);
    }
};
INLINE auto Type::as_variant () const {
    return get_extended_type<VariantType>(this);
}


struct StructField {
    
    struct Data : TypeContainer {
        StringSection<uint16_t> name;

        INLINE Type* type () {
            return reinterpret_cast<Type*>(this + 1);
        }
    };

    INLINE Data* data () {
        return get_padded<Data>(reinterpret_cast<size_t>(this));
    }
};

struct EnumField {
    uint64_t value = 0;
    StringSection<uint16_t> name;
    
    bool is_negative = false;

    INLINE EnumField* next () {
        return this + 1;
    }
};

template <typename T, KEYWORDS keyword>
INLINE std::pair<T*, IdentifedDefinitionIndex> create_extended_identified_definition (Buffer &buffer) {
    auto padding = get_padding<T>(buffer.current_position() + sizeof_v<IdentifiedDefinition>);
    IdentifedDefinitionIndex def_idx = buffer.get_next_multi_byte<IdentifiedDefinition>(sizeof_v<IdentifiedDefinition> + padding + sizeof_v<T>);
    IdentifiedDefinition* def = buffer.get(def_idx);
    def->keyword = keyword;
    Buffer::Index<T> extended_idx = Buffer::Index<T>{static_cast<uint32_t>(def_idx.value + sizeof_v<IdentifiedDefinition> + padding)};
    T* extended = buffer.get(extended_idx);
    return std::pair(extended, def_idx);
}

template <KEYWORDS keyword>
requires (keyword == STRUCT || keyword == UNION)
struct DefinitionWithFields : IdentifiedDefinition::Data {
    uint16_t field_count;
    bool is_fixed_size;

    INLINE static auto create(Buffer &buffer) {
        std::pair<DefinitionWithFields*, IdentifedDefinitionIndex> result = create_extended_identified_definition<DefinitionWithFields, keyword>(buffer);
        return result;
    }

    INLINE StructField::Data* reserve_field(Buffer &buffer) {
        size_t padding = get_padding<StructField::Data>(buffer.current_position());
        auto idx = buffer.get_next_multi_byte<StructField::Data>(sizeof_v<StructField::Data> + padding);
        return buffer.get_aligned(idx.add(padding));
    }

    INLINE StructField* first_field() {
        return reinterpret_cast<StructField*>(this + 1);
    }
};

auto a = sizeof_v<DefinitionWithFields<STRUCT>>;


typedef DefinitionWithFields<STRUCT> StructDefinition;
typedef DefinitionWithFields<UNION> UnionDefinition;

struct EnumDefinition : IdentifiedDefinition::Data {
    uint16_t field_count;
    SIZE type_size;

    INLINE static auto create(Buffer &buffer) {
        std::pair<EnumDefinition*, IdentifedDefinitionIndex> result = create_extended_identified_definition<EnumDefinition, ENUM>(buffer);
        return result;
    }

    INLINE EnumField* reserve_field(Buffer &buffer) {
        return buffer.get_next_aligned<EnumField>();
    }

    INLINE EnumField* first_field() {
        return reinterpret_cast<EnumField*>(this + 1);
    }
};

/* struct TypedefDefinition : IdentifiedDefinition::Data, TypeContainer {
    INLINE static auto create(Buffer &buffer) {
        return create_extended_identified_definition<TypedefDefinition, TYPEDEF>(buffer);
    }

    INLINE Type* type() {
        return reinterpret_cast<Type*>(this + 1);
    }
}; */

template <typename T>
INLINE T* get_extended_identifed_definition(auto* that) {
    return get_extended<T, IdentifiedDefinition>(that);
}

INLINE IdentifiedDefinition::Data* IdentifiedDefinition::data () {
    return get_extended_identifed_definition<Data>(this);
}

INLINE constexpr auto IdentifiedDefinition::Data::as_struct () {
    return static_cast<StructDefinition*>(this);
}
INLINE constexpr auto IdentifiedDefinition::Data::as_enum () {
    return static_cast<EnumDefinition*>(this);
}
INLINE constexpr auto IdentifiedDefinition::Data::as_union () {
    return static_cast<UnionDefinition*>(this);
}
/* INLINE constexpr auto IdentifiedDefinition::Data::as_typedef () {
    return static_cast<TypedefDefinition*>(this);
} */

typedef std::unordered_map<std::string, IdentifedDefinitionIndex> IdentifierMap;

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

INLINE void add_identifier (std::pair<char*, char*> name, IdentifierMap &identifier_map, IdentifiedDefinition::Data* definition_data, IdentifedDefinitionIndex definition_idx) {
    auto [start, end] = name;
    size_t length = end - start;
    if (length > std::numeric_limits<uint16_t>::max()) {
        show_syntax_error("identifier name too long", start);
    }
    definition_data->name = StringSection<uint16_t>{start, static_cast<uint16_t>(length)};

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

LexTypeResult lex_type (char* YYCURSOR, Buffer &buffer, TypeContainer* type_container, IdentifierMap &identifier_map) {
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
                uint32_t delta = range.max - range.min;
                if (delta <= UINT8_MAX) {
                    leaf_counts = {1, 0, 0, 0};
                    StringType::create<SIZE_1>(buffer, range.min);
                } else if (delta <= UINT16_MAX) {
                    leaf_counts = {0, 1, 0, 0};
                    StringType::create<SIZE_2>(buffer, range.min);
                } else /* if (delta <= UINT32_MAX) */ {
                    leaf_counts = {0, 0, 1, 0};
                    StringType::create<SIZE_4>(buffer, range.min);
                }
                internal_size = sizeof_v<Type> + alignof(StringType) - 1 + sizeof_v<StringType>; 
            }
        );

        
        return {YYCURSOR, leaf_counts, internal_size, is_fixed_size};
    }

    array: {
        auto [extended, base] = ArrayType::create(buffer);

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        YYCURSOR = skip_white_space(YYCURSOR);

        auto result = lex_type(YYCURSOR, buffer, extended, identifier_map);
        if (!result.is_fixed_size) {
            show_syntax_error("expected static size type", YYCURSOR);
        }
        YYCURSOR = result.cursor;

        YYCURSOR = lex_same_line_symbol<',', "expected length argument">(YYCURSOR);

        bool is_fixed_size;
        LeafCounts leaf_counts;

        YYCURSOR = lex_range_argument(
            YYCURSOR, 
            [&buffer, &extended, &base, &is_fixed_size, &leaf_counts, &result](uint32_t length) {
                base->type = ARRAY_FIXED;
                extended->length = length;
                extended->fixed_alignment = SIZE_0;
                leaf_counts = result.leaf_counts;
                is_fixed_size = true;
            },
            [&buffer, &extended, &base, &is_fixed_size, &leaf_counts](Range range) {
                base->type = ARRAY;
                uint32_t delta = range.max - range.min;
                if (delta <= UINT8_MAX) {
                    extended->fixed_alignment = SIZE_1;
                    leaf_counts = {1, 0, 0, 0};
                } else if (delta <= UINT16_MAX) {
                    extended->fixed_alignment = SIZE_2;
                    leaf_counts = {0, 1, 0, 0};
                } else /* if (delta <= UINT32_MAX) */ {
                    extended->fixed_alignment = SIZE_4;
                    leaf_counts = {0, 0, 1, 0};
                }
                extended->length = range.min;
                is_fixed_size = false;
            }
        );

        return {YYCURSOR, leaf_counts, static_cast<uint32_t>(result.internal_size + sizeof_v<Type> + sizeof_v<ArrayType> + alignof(ArrayType) - 1), is_fixed_size};
    }

    variant: {

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        auto variant_type = VariantType::create(buffer);
        variant_count_t variant_count = 0;


        uint32_t internal_size = sizeof_v<Type> + alignof(VariantType) - 1 + sizeof_v<VariantType>;
        LeafCounts leaf_counts = {0, 0, 0, 0};
        while (1) {
            variant_count++;
            YYCURSOR = skip_white_space(YYCURSOR);
            auto result = lex_type(YYCURSOR, buffer, variant_type, identifier_map);
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

        variant_type->variant_count = variant_count;

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

template <KEYWORDS keyword, bool is_first_field>
requires (keyword == STRUCT || keyword == UNION)
INLINE char* lex_struct_or_union_fields (
    char* YYCURSOR,
    DefinitionWithFields<keyword>* definition,
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
            definition->leaf_counts = leaf_counts;
            definition->internal_size = internal_size;
            definition->field_count = field_count;
            definition->is_fixed_size = is_fixed_size;
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
        StructField::Data* field = definition->first_field()->data();
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
        StructField::Data* field = definition->reserve_field(buffer);
        field->name = {start, static_cast<uint16_t>(length)};
        
        YYCURSOR = skip_white_space(YYCURSOR);
        auto result = lex_type(YYCURSOR, buffer, field, identifier_map);
        YYCURSOR = result.cursor;
        internal_size += result.internal_size;
        leaf_counts += result.leaf_counts;

        YYCURSOR = lex_same_line_symbol<';'>(YYCURSOR);

        if constexpr (is_first_field) {
            return lex_struct_or_union_fields<keyword, false>(YYCURSOR, definition, identifier_map, buffer, leaf_counts, internal_size, 1, result.is_fixed_size);
        } else {
            is_fixed_size &= result.is_fixed_size;
            goto before_field;
        }
    }
    
}

template <KEYWORDS keyword>
requires (keyword == STRUCT || keyword == UNION)
INLINE char* lex_struct_or_union(
    char* YYCURSOR,
    DefinitionWithFields<keyword>* definition,
    IdentifierMap &identifier_map,
    Buffer &buffer
) {
    YYCURSOR = lex_same_line_symbol<'{', "expected '{'">(YYCURSOR);

    return lex_struct_or_union_fields<keyword, true>(YYCURSOR, definition, identifier_map, buffer, {0, 0, 0, 0}, sizeof_v<IdentifiedDefinition> + alignof(DefinitionWithFields<keyword>) - 1 + sizeof_v<DefinitionWithFields<keyword>>, 0, true);
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

INLINE auto add_member (EnumDefinition* definition, Buffer &buffer, char* start, char* end, uint64_t value, bool is_negative) {
    auto field = definition->reserve_field(buffer);
    field->name = {start, end};
    field->value = value;
    field->is_negative = is_negative;
}

template <bool is_signed>
INLINE char* lex_enum_fields (
    char* YYCURSOR,
    EnumDefinition* definition,
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
            definition->field_count = field_count;
            constexpr auto a = std::numeric_limits<int16_t>::min();
            if (max_value_unsigned <= UINT8_MAX) {
                definition->type_size = SIZE_1;
            } else if (max_value_unsigned <= UINT16_MAX) {
                definition->type_size = SIZE_2;
            } else if (max_value_unsigned <= UINT32_MAX) {
                definition->type_size = SIZE_4;
            } else {
                definition->type_size = SIZE_8;
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
            auto field = definition->first_field();
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
                        add_member(definition, buffer, start, end, value, is_negative);
                        return lex_enum_fields<true>(YYCURSOR, definition, field_count, value, max_value_unsigned, is_negative, identifier_map, buffer);
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
            add_member(definition, buffer, start, end, value, is_negative);
            goto enum_end;
        }

        enum_member: {
            field_count++;
            add_member(definition, buffer, start, end, value, is_negative);
        }
    }
}

INLINE char* lex_enum (
    char* YYCURSOR,
    EnumDefinition* definition,
    IdentifierMap &identifier_map,
    Buffer &buffer
) {
    YYCURSOR = lex_same_line_symbol<'{', "expected '{'">(YYCURSOR);

    uint16_t field_count = 0;

    uint64_t value = 1;
    bool is_negative = true;

    uint64_t max_value_unsigned = 0;

    return lex_enum_fields<false>(YYCURSOR, definition, field_count, value, max_value_unsigned, is_negative, identifier_map, buffer);
}

INLINE IdentifiedDefinition* lex (char* YYCURSOR, IdentifierMap &identifier_map, Buffer &buffer) {
    IdentifiedDefinition* target = nullptr;
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
        auto [definition_data, definition_idx] = StructDefinition::create(buffer);
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        YYCURSOR = lex_struct_or_union(YYCURSOR, definition_data, identifier_map, buffer);
        add_identifier(name_result.value, identifier_map, definition_data, definition_idx);
        goto loop;
    }
    enum_keyword: {
        auto [definition_data, definition_idx] = EnumDefinition::create(buffer);
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        YYCURSOR = lex_enum(YYCURSOR, definition_data, identifier_map, buffer);
        add_identifier(name_result.value, identifier_map, definition_data, definition_idx);
        goto loop;
    }
    union_keyword: {
        auto [definition_data, definition_idx] = UnionDefinition::create(buffer);
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        YYCURSOR = lex_struct_or_union(YYCURSOR, definition_data, identifier_map, buffer);
        add_identifier(name_result.value, identifier_map, definition_data, definition_idx);
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
        YYCURSOR = skip_white_space(YYCURSOR);
        auto type_container = buffer.get_next<TypeContainer>();
        YYCURSOR = lex_type(YYCURSOR, buffer, type_container, identifier_map).cursor;
        YYCURSOR = lex_same_line_symbol<';'>(YYCURSOR);
        auto type = (Type*)type_container;
        if (type->type != FIELD_TYPE::IDENTIFIER) {
            INTERNAL_ERROR("target must be an identifier");
        }
        target = buffer.get(type->as_identifier()->identifier_idx);
        goto loop;
    }

    eof: {
        if (target) {
            return target;
        }
        INTERNAL_ERROR("no target defined");
    }
}

}