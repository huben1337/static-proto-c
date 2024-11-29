#pragma once
#include <cstdint>
#include <utility>
#include <unordered_map>
#include "string_helpers.cpp"
#include "base.cpp"
#include "memory.cpp"
#include "lex_result.cpp"
#include "lex_error.cpp"
#include "lex_helpers.re2c.cpp"
#include "parse_int.re2c.cpp"
#include "helper_types.cpp"

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
    INT8,
    INT16,
    INT32,
    INT64,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    FLOAT32,
    FLOAT64,
    BOOL,
    FIXED_STRING,
    STRING,
    IDENTIFIER,
    ARRAY,
    VARIANT
};

struct Range {
    uint32_t min;
    uint32_t max;
};

typedef uint16_t variant_count_t;


struct IdentifiedDefinition {
    KEYWORDS keyword;

    struct Data {
        StringSection<uint16_t> name;

        INLINE constexpr auto as_struct ();
        INLINE constexpr auto as_enum ();
        INLINE constexpr auto as_union ();
        INLINE constexpr auto as_typedef ();
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

template <typename T>
INLINE size_t get_padding(size_t position) {
    size_t mod = position % alignof(T);
    size_t padding = (alignof(T) - mod) & (alignof(T) - 1);
    return padding;
}

template <typename T>
INLINE T* get_padded (auto* that) {
    size_t address = reinterpret_cast<size_t>(that);
    size_t padding = get_padding<T>(address);
    return std::assume_aligned<alignof(T)>(reinterpret_cast<T*>(address + padding));
}

template <typename T>
INLINE T* get_padded (size_t address) {
    size_t padding = get_padding<T>(address);
    return std::assume_aligned<alignof(T)>(reinterpret_cast<T*>(address + padding));
}

template <typename T, FIELD_TYPE type>
INLINE T* create_extended_type (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position() + sizeof_v<Type>);
    Buffer::Index<Type> type_idx = buffer.get_next_multi_byte<Type>(sizeof_v<Type> + padding + sizeof_v<T>);
    Type* __type = buffer.get(type_idx);
    __type->type = type;
    auto extended_idx = Buffer::Index<T>{static_cast<uint32_t>(type_idx.value + padding + sizeof_v<Type>)};
    return buffer.get_aligned(extended_idx);
}


template <typename T, typename U>
INLINE T* get_extended(auto* that) {
    return get_padded<T>(reinterpret_cast<size_t>(that) + sizeof_v<U>);
}

template <typename T>
INLINE T* get_extended_type(auto* that) {
    return get_extended<T, Type>(that);
}

struct FixedStringType {
    INLINE static void create (Buffer &buffer, uint32_t length) {
        auto type = create_extended_type<FixedStringType, FIXED_STRING>(buffer);
        type->length = length;
    }

    uint32_t length;
};
INLINE auto Type::as_fixed_string () const {
    return get_extended_type<FixedStringType>(this);
}

struct StringType {
    INLINE static void create (Buffer &buffer, Range range) {
        auto type = create_extended_type<StringType, STRING>(buffer);
        type->range = range;
    }

    Range range;
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

    INLINE static ArrayType* create (Buffer &buffer) {
        return create_extended_type<ArrayType, ARRAY>(buffer);
    }

    Range range;

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

    INLINE static auto create(Buffer &buffer) {
        std::pair<DefinitionWithFields*, IdentifedDefinitionIndex> result = create_extended_identified_definition<DefinitionWithFields, keyword>(buffer);
        result.first->field_count = 0;
        return result;
    }

    INLINE StructField::Data* reserve_field(Buffer &buffer) {
        field_count++;
        size_t padding = get_padding<StructField::Data>(buffer.current_position());
        auto idx = buffer.get_next_multi_byte<StructField::Data>(sizeof_v<StructField::Data> + padding);
        return buffer.get_aligned(idx.add(padding));
    }

    INLINE StructField* first_field() {
        return reinterpret_cast<StructField*>(this + 1);
    }
};



typedef DefinitionWithFields<STRUCT> StructDefinition;
typedef DefinitionWithFields<UNION> UnionDefinition;

struct EnumDefinition : IdentifiedDefinition::Data {
    uint16_t field_count;

    INLINE static auto create(Buffer &buffer) {
        std::pair<EnumDefinition*, IdentifedDefinitionIndex> result = create_extended_identified_definition<EnumDefinition, ENUM>(buffer);
        result.first->field_count = 0;
        return result;
    }

    INLINE EnumField* reserve_field(Buffer &buffer) {
        field_count++;
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


char* lex_type (char* YYCURSOR, Buffer &buffer, TypeContainer* type_container, IdentifierMap &identifier_map) {
    const char* identifier_start = YYCURSOR;
    #define SIMPLE_TYPE(TYPE) buffer.get_next<Type>()->type = FIELD_TYPE::TYPE; return YYCURSOR;
    /*!local:re2c
        name = [a-zA-Z_][a-zA-Z0-9_]*;
                    
        "int8"      { SIMPLE_TYPE(INT8)       }
        "int16"     { SIMPLE_TYPE(INT16)      }
        "int32"     { SIMPLE_TYPE(INT32)      }
        "int64"     { SIMPLE_TYPE(INT64)      }
        "uint8"     { SIMPLE_TYPE(UINT8)      }
        "uint16"    { SIMPLE_TYPE(UINT16)     }
        "uint32"    { SIMPLE_TYPE(UINT32)     }
        "uint64"    { SIMPLE_TYPE(UINT64)     }
        "float32"   { SIMPLE_TYPE(FLOAT32)    }
        "float64"   { SIMPLE_TYPE(FLOAT64)    }
        "bool"      { SIMPLE_TYPE(BOOL)       }
        "string"    { goto string;            }
        "array"     { goto array;             }
        "variant"   { goto variant;           }
        name        { goto identifier;        }

        * { UNEXPECTED_INPUT("expected type"); }
    */
    #undef SIMPLE_TYPE

    string: {

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        YYCURSOR = lex_range_argument(
            YYCURSOR,
            [&buffer](uint32_t value) {
                FixedStringType::create(buffer, value);
            },
            [&buffer](Range range) {
                StringType::create(buffer, range);
            }
        );
        return YYCURSOR;
    }

    array: {
        auto array_type = ArrayType::create(buffer);

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        YYCURSOR = skip_white_space(YYCURSOR);

        YYCURSOR = lex_type(YYCURSOR, buffer, array_type, identifier_map);

        YYCURSOR = lex_same_line_symbol<',', "expected length argument">(YYCURSOR);

        YYCURSOR = lex_range_argument(
            YYCURSOR, 
            [&buffer, &array_type](uint32_t value) {
                array_type->range = Range{value, value};
            },
            [&buffer, &array_type](Range range) {
                array_type->range = range;
            }
        );
        return YYCURSOR;
    }

    variant: {

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        auto variant_type = VariantType::create(buffer);
        variant_count_t variant_count = 0;

        while (1) {
            variant_count++;
            YYCURSOR = skip_white_space(YYCURSOR);
            YYCURSOR = lex_type(YYCURSOR, buffer, variant_type, identifier_map);

            /*!local:re2c
                white_space* "," { continue; }
                white_space* ">" { break; }
                * { UNEXPECTED_INPUT("expected ',' or '>'"); }
            */
            
        }

        variant_type->variant_count = variant_count;

        return YYCURSOR;
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
        IdentifiedType::create(buffer, identifier_idx_iter->second);
        return YYCURSOR;
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
    return (T*)type;
}

template <typename T>
T* skip_type (Type* type) {
    switch (type->type)
    {
    case FIXED_STRING:
        return reinterpret_cast<T*>(type->as_fixed_string() + 1);
    case STRING: {
        return reinterpret_cast<T*>(type->as_string() + 1);
    case IDENTIFIER:
        return reinterpret_cast<T*>(type->as_identifier() + 1);
    case ARRAY: {
        return skip_type<T>(type->as_array()->inner_type());
    }
    case VARIANT: {
        return skip_variant_type<T>(type->as_variant());
    }
    default:
        return reinterpret_cast<T*>(type + 1);
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
    
    while (1) {
        YYCURSOR = skip_any_white_space(YYCURSOR);

        /*!local:re2c
            [a-zA-Z_]   { goto name_start; }
            "}"         { goto struct_end; }

            * { UNEXPECTED_INPUT("expected field name or '}'"); }
        */

        struct_end: {
            if (definition->field_count == 0) {
                show_syntax_error("expected at least one field", YYCURSOR - 1);
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
        if (length > std::numeric_limits<uint16_t>::max()) {
            UNEXPECTED_INPUT("field name too long");
        }
        {
            auto field_count = definition->field_count;
            if (field_count > 0) {
                StructField::Data* field = definition->first_field()->data();
                for (uint32_t i = 0; i < definition->field_count; i++) {
                    if (string_section_eq(field->name.offset, field->name.length, start, length)) {
                        show_syntax_error("field already defined", start);
                    }
                    field = skip_type<StructField>(field->type())->data();
                }
            }
        }
        /*!local:re2c
            white_space* ":" { goto struct_field; }
            * { UNEXPECTED_INPUT("expected ':'"); }
        */

        struct_field:
        StructField::Data* field = definition->reserve_field(buffer);
        field->name = {start, static_cast<uint16_t>(length)};
        
        YYCURSOR = skip_white_space(YYCURSOR);
        YYCURSOR = lex_type(YYCURSOR, buffer, field, identifier_map);

        field_end:
        YYCURSOR = lex_same_line_symbol<';'>(YYCURSOR);
    }
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

INLINE char* lex_enum (
    char* YYCURSOR,
    EnumDefinition* definition,
    IdentifierMap &identifier_map,
    Buffer &buffer
) {
    YYCURSOR = lex_same_line_symbol<'{', "expected '{'">(YYCURSOR);

    uint64_t value = 1;
    bool is_negative = true;

    while (1) {
        YYCURSOR = skip_any_white_space(YYCURSOR);

        /*!local:re2c
            [a-zA-Z_]   { goto name_start; }
            "}"         { goto enum_end; }

            * { UNEXPECTED_INPUT("expected field name or '}'"); }
        */

        enum_end: {
            if (definition->field_count == 0) {
                show_syntax_error("expected at least one member", YYCURSOR - 1);
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
            for (uint32_t i = 0; i < definition->field_count; i++) {
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
            LexResult<uint64_t> parsed;
            if (is_negative) {
                parsed = parse_int<uint64_t, std::numeric_limits<int64_t>::max()>(YYCURSOR + 1);
            } else {
                parsed = parse_int<uint64_t>(YYCURSOR);
            }
            value = parsed.value;
            YYCURSOR = parsed.cursor;

            /*!local:re2c
                white_space* "," { goto enum_member; }
                white_space* "}" { goto last_member; }
                * { UNEXPECTED_INPUT("expected ',' or end of enum definition"); }
            */
        }

        default_last_member: {
            std::tie(value, is_negative) = set_member_value(start, value, is_negative);
            goto last_member;
        }

        last_member: {
            add_member(definition, buffer, start, end, value, is_negative);
            goto enum_end;
        }

        enum_member: {
            add_member(definition, buffer, start, end, value, is_negative);
        }
    }
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
        YYCURSOR = lex_type(YYCURSOR, buffer, type_container, identifier_map);
        YYCURSOR = lex_same_line_symbol<';'>(YYCURSOR);
        auto type = (Type*)type_container;
        if (type->type != FIELD_TYPE::IDENTIFIER) {
            INTERNAL_ERROR("target must be an identifier");
        }
        target = buffer.get(type->as_identifier()->identifier_idx);
        goto loop;
    }

    eof: {
        if(target) {
            return target;
        }
        INTERNAL_ERROR("no target defined");
    }
}