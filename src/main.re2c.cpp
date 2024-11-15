#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <unordered_map>
#include <stdarg.h>
#include <vector>

#include "string_literal.cpp"
#include "helper_types.cpp"
#include "string_helpers.cpp"


template <typename T>
__forceinline auto add_offset(void* start, size_t offset) {
    return (T*)((uint8_t*)start + offset);
}

#define UNEXPECTED_INPUT(msg) show_input_error(msg, YYCURSOR); exit(1);

#define INTERNAL_ERROR(msg) printf(msg); exit(1);



template <typename AEnd, typename BEnd>
requires (std::is_integral_v<AEnd> || is_char_ptr_t<AEnd>) && (std::is_integral_v<BEnd> || is_char_ptr_t<BEnd>)
__forceinline bool string_section_eq(const char* a_start, AEnd a_end_or_length, const char* b_start, BEnd b_end_or_length) {
    size_t a_length = 0;
    size_t b_length = 0;
    
    if constexpr (is_char_ptr_t<AEnd>) {
        a_length = a_end_or_length - a_start;
    } else {
        a_length = a_end_or_length;
    }
    
    if constexpr (is_char_ptr_t<BEnd>) {
        b_length = b_end_or_length - b_start;
    } else {
        b_length = b_end_or_length;
    }
    

    if (a_length != b_length) return false;
    for (size_t i = 0; i < a_length; i++) {
        if (a_start[i] != b_start[i]) return false;
    }
    
    return true;
}


const char *input_start;
std::string file_path_string;

void show_input_error (const char *msg, const char *error, char *error_end = 0) {
    if (!input_start) {
        INTERNAL_ERROR("input_start not set\n");
    }
    if (file_path_string.empty()) {
        INTERNAL_ERROR("file_name not set\n");
    }

    const char *start = error;
    while (1) {
        if (start == input_start) break;
        if (*start == '\n') {
            start++;
            break;
        }
        start--;
    }

    uint64_t line = 0;
    for (auto i = input_start; i < start; i++) {
        if (*i == '\n') {
            line++;
        }
    }

    uint64_t column = error - start;

    const char *end = error;
    while (1) {
        if (*end == 0) goto print;
        if (*end == '\n') break; 
        end++;
    }
    print:
    printf("\n\033[97m%s:%d:%d\033[0m \033[91merror:\033[97m %s\033[0m\n  %s\n\033[%dC\033[31m^\033[0m", file_path_string.c_str(), line + 1, column + 1, msg, extract_string(start, end).c_str(), column + 2);
    exit(1);
}

/*!re2c
    re2c:tags = 1;
    re2c:define:YYMARKER = YYCURSOR;
    re2c:yyfill:enable = 0;
    re2c:define:YYCTYPE = char;

    any_white_space = [ \t\r\n];
    white_space = [ \t];
    end = "\x00";
*/

enum KEYWORDS : uint8_t {
    STRUCT,
    ENUM,
    UNION,
    TYPEDEF,
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
    STRING,
    IDENTIFIER,
    ARRAY,
    VARIANT
};



struct Range {
    uint32_t min;
    uint32_t max;
};


template <typename U, U max = std::numeric_limits<U>::max()>
requires std::is_integral_v<U> && std::is_unsigned_v<U>
struct Memory {
    private:
    U capacity;
    U position = 0;
    uint8_t* memory;
    bool in_heap;

    public:
    Memory (U capacity) : capacity(capacity), in_heap(true) {
        memory = static_cast<uint8_t*>(std::malloc(capacity));
        if (!memory) {
            INTERNAL_ERROR("memory allocation failed\n");
        }
    }

    template <U N>
    Memory (uint8_t (&memory)[N]) : capacity(N), memory(memory), in_heap(false) {}

    using index_t = U;

    template <typename T>
    struct Index {
        U value;
    };


    uint8_t* c_memory () {
        return memory;
    }

    U current_position () {
        return position;
    }

    template <typename T>
    T* get (Index<T> index) {
        return (T*)(memory + index.value);
    }


    template <typename T>
    Index<T> get_next () {
        if constexpr (sizeof(T) == 1) {
            return get_next_single_byte<T>();
        } else {
            return get_next_multi_byte<T>();
        }
    }

    template <typename T>
    requires (sizeof(T) == 1)
    Index<T> get_next_single_byte () {
        if (position == capacity) {
            if (capacity >= (max / 2)) {
                INTERNAL_ERROR("memory overflow\n");
            }
            capacity *= 2;
            grow(capacity);
        }
        return Index<T>{position++};
    }

    template <typename T>
    requires (sizeof(T) > 1)
    Index<T> get_next_multi_byte () {
        U next = position;
        position += sizeof(T);
        if (position < next) {
            INTERNAL_ERROR("[Memory] position wrapped\n");
        }
        if (position >= capacity) {
            if (position >= (max / 2)) {
                INTERNAL_ERROR("memory overflow\n");
            }
            capacity = position * 2;
            grow(capacity);
        }
        return Index<T>{next};
    }

    void clear () {
        position = 0;
    }

    void free () {
        if (in_heap) {
            std::free(memory);
        }
    }

    private:
    void grow (U size) {
        if (in_heap) {
            memory = (uint8_t*) std::realloc(memory, capacity);
            // printf("reallocated memory in heap: to %d\n", capacity);
        } else {
            auto new_memory = (uint8_t*) std::malloc(capacity);
            memcpy(new_memory, memory, position);
            memory = new_memory;
            in_heap = true;
            // printf("reallocated memory from stack: to %d\n", capacity);
        }
        if (!memory) INTERNAL_ERROR("memory allocation failed\n");
    }

};

typedef Memory<uint32_t> Buffer;

typedef uint32_t struct_field_idx_t;
typedef uint32_t enum_field_idx_t;
typedef uint32_t range_idx_t;
typedef uint32_t type_idx_t;
typedef uint16_t identifier_idx_t;
typedef uint16_t variant_count_t;

struct IdentifiedDefinition {
    StringSection<uint16_t> name;
    KEYWORDS keyword;

    constexpr auto as_struct () const;
    constexpr auto as_enum () const;
    constexpr auto as_union () const;
    constexpr auto as_typedef () const;
};


struct Type {
    FIELD_TYPE type;

    constexpr auto as_string () const;
    constexpr auto as_identifier () const;
    constexpr auto as_array () const;
    constexpr auto as_variant () const;
};

struct TypeContainer {
    template <typename T>
    requires std::is_base_of_v<Type, T>
    T* reserve_type (Buffer &buffer) {
        auto idx = buffer.get_next<T>();
        return buffer.get(idx);
    }
};

struct StringType : Type {
    StringType (Range range) : Type({STRING}), range(range) {}
    Range range;
};

constexpr auto Type::as_string () const {
    return (StringType*)this;
}


struct IdentifiedType : Type {
	IdentifiedType (Buffer::Index<IdentifiedDefinition> identifier_idx) : Type({IDENTIFIER}), identifier_idx(identifier_idx) {}
    Buffer::Index<IdentifiedDefinition> identifier_idx;
};


constexpr auto Type::as_identifier () const {
    return (IdentifiedType*)this;
}

struct ArrayType : Type, TypeContainer {
    Range range;

    auto inner_type () {
        return (Type*)(this + 1);
    }
};

constexpr auto Type::as_array () const {
    return (ArrayType*)this;
}

struct VariantType : Type, TypeContainer {
	VariantType (variant_count_t variant_count) :
        Type({VARIANT}),
        variant_count(variant_count)
    {};
    variant_count_t variant_count;

    Type *first_variant() {
        return (Type*)(this + 1);
    }
};

constexpr auto Type::as_variant () const {
    return (VariantType*)this;
}


struct StructField : TypeContainer {
    StringSection<uint16_t> name;

    Type *type () {
        return (Type*)(this + 1);
    }
};

struct EnumField {
    uint64_t value = 0;
    StringSection<uint16_t> name;
    
    bool is_negative = false;

    auto next () {
        return this + 1;
    }
};




template <typename T>
struct DefinitionWithFields : IdentifiedDefinition {
    // using T = std::conditional_t<kw == STRUCT || kw == UNION, std::conditional_t<kw == ENUM, EnumField, <error-type>>;
    uint16_t field_count;

    static auto create(Buffer &buffer) {
        auto idx = buffer.get_next<DefinitionWithFields>();
        DefinitionWithFields* ptr = buffer.get(idx);
        ptr->field_count = 0;
        return std::pair(ptr, idx);
    }

    T* reserve_field(Buffer &buffer) {
        field_count++;
        return buffer.get(buffer.get_next<T>());
    }

    T* first_field() {
        return (T*)(this + 1);
    }
};

typedef DefinitionWithFields<StructField> StructDefinition;
typedef DefinitionWithFields<StructField> UnionDefinition;
typedef DefinitionWithFields<EnumField> EnumDefinition;

struct TypedefDefinition : IdentifiedDefinition, TypeContainer {
    static auto create(Buffer &buffer) {
        auto idx = buffer.get_next<TypedefDefinition>();
        auto ptr = buffer.get(idx);
        return std::pair(ptr, idx);
    }

    Type* type() {
        return (Type*)(this + 1);
    }
};


constexpr auto IdentifiedDefinition::as_struct () const {
    return (StructDefinition*)this;
}
constexpr auto IdentifiedDefinition::as_enum () const {
    return (EnumDefinition*)this;
}
constexpr auto IdentifiedDefinition::as_union () const {
    return (UnionDefinition*)this;
}
constexpr auto IdentifiedDefinition::as_typedef () const {
    return (TypedefDefinition*)this;
}




typedef std::unordered_map<std::string, Buffer::Index<IdentifiedDefinition>> IdentifierMap;

#define VALUE_VAR value
#define RETURN_RESULT return { YYCURSOR - 1, is_negative ? -VALUE_VAR : VALUE_VAR };
#define CHECK_RANGE if (VALUE_VAR > max) { UNEXPECTED_INPUT("value out of range"); }
#define PUSH_DEC(VAL) VALUE_VAR = VALUE_VAR * 10 + VAL; CHECK_RANGE
#define PUSH_BIN(VAL) VALUE_VAR = (VALUE_VAR << 1) | VAL; CHECK_RANGE
#define PUSH_OCT(VAL) VALUE_VAR = (VALUE_VAR << 3) | VAL; CHECK_RANGE
#define PUSH_HEX(VAL) VALUE_VAR = (VALUE_VAR << 4) | VAL; CHECK_RANGE

template <typename T>
struct LexResult {
    char *cursor;
    T value;
};

/**
 * \brief Parses an integer from the input string.
 *
 * This function attempts to parse an integer from the given input string
 * starting at the position pointed to by YYCURSOR. It supports parsing
 * integers in decimal, binary, octal, and hexadecimal formats. The function
 * handles optional negative signs for signed integer types.
 *
 * \tparam T The integer type to parse, which must be an integral type.
 * \param YYCURSOR Pointer to the current position in the input string.
 * \returns A LexResult containing the parsed integer value and the updated
 *          cursor position. If the input does not represent a valid integer,
 *          an error is triggered.
 */
template <typename T, const T max = std::numeric_limits<T>::max()>
requires std::is_integral_v<T>
LexResult<T> parse_int (char *YYCURSOR) {
    T VALUE_VAR = 0;
    bool is_negative = false;

    if constexpr (std::is_signed_v<T>) {
        /*!local:re2c
            "-"                { goto minus_sign; }
            "0b"               { goto bin_entry; }
            "0"                { goto oct; }
            [1-9]              { PUSH_DEC(yych - '0'); goto dec; }
            "0x"               { goto hex_entry; }
            *                  { UNEXPECTED_INPUT("expected integer literal"); }
        */
        minus_sign:
        is_negative = true;
    }

    /*!local:re2c
        "0b"               { goto bin_entry; }
        "0"                { goto oct; }
        [1-9]              { PUSH_DEC(yych - '0'); goto dec; }
        "0x"               { goto hex_entry; }
        *                  { UNEXPECTED_INPUT("expected unsigned integer literal"); }
    */
    
bin_entry:
    /*!local:re2c
        [01]        { PUSH_BIN(yych - '0'); goto bin; }
        *           { UNEXPECTED_INPUT("expected binary digit"); }
    */
bin:
    /*!local:re2c
        [01]        { PUSH_BIN(yych - '0'); goto bin; }
        *           { RETURN_RESULT; }
    */
oct:
    /*!local:re2c
        [0-7]       { PUSH_OCT(yych - '0'); goto oct; }
        *           { RETURN_RESULT; }
    */
dec:
    /*!local:re2c
        [0-9]       { PUSH_DEC(yych - '0'); goto dec; }
        *           { RETURN_RESULT; }
    */
hex_entry:
    /*!local:re2c
        [0-9]       { PUSH_HEX(yych - '0');      goto hex; }
        [a-f]       { PUSH_HEX(yych - 'a' + 10); goto hex; }
        [A-F]       { PUSH_HEX(yych - 'A' + 10); goto hex; }
        *           { UNEXPECTED_INPUT("expected hex digit"); }
    */
hex:
    /*!local:re2c
        [0-9]       { PUSH_HEX(yych - '0');      goto hex; }
        [a-f]       { PUSH_HEX(yych - 'a' + 10); goto hex; }
        [A-F]       { PUSH_HEX(yych - 'A' + 10); goto hex; }
        *           { RETURN_RESULT; }
    */
}
#undef RETURN_RESULT
#undef VALUE_VAR


#define CHECK_SYMBOL                                                \
if (yych == symbol) {                                               \
    return ++YYCURSOR;                                              \
} else {                                                            \
    UNEXPECTED_INPUT(error_message.value)    \
}

template<char symbol>
constexpr auto lex_symbol_error = StringLiteral({'e','x','p','e', 'c', 't', 'e', 'd', ' ', 's', 'y', 'm', 'b', 'o', 'l', ':', ' ', symbol});


/**
 * \brief Lexes a specific symbol, allowing any amount of whitespace before it.
 *
 * This function attempts to lex a specified symbol in the input string
 * starting at the position pointed to by YYCURSOR. It skips over any amount
 * of whitespace before checking for the symbol.
 *
 * \tparam symbol The character symbol to lex.
 * \param YYCURSOR Pointer to the current position in the input.
 * \returns A pointer to the position immediately following the symbol,
 *          or triggers an error if the symbol is not found.
 */
template <char symbol, StringLiteral error_message = lex_symbol_error<symbol>>
__forceinline char *lex_symbol (char *YYCURSOR) {
    /*!local:re2c
        any_white_space* { CHECK_SYMBOL }
    */
}

/**
 * \brief Lexes a specific symbol on the same line.
 *
 * This function attempts to lex a specified symbol that is expected to appear
 * on the same line as the current position in the input. It skips over any
 * leading whitespace before checking for the symbol.
 *
 * \tparam symbol The character symbol to lex.
 * \param YYCURSOR Pointer to the current position in the input.
 * \returns A pointer to the position immediately following the symbol,
 *          or triggers an error if the symbol is not found.
 */
template <char symbol, StringLiteral error_message = lex_symbol_error<symbol>>
__forceinline char *lex_same_line_symbol (char *YYCURSOR) {
    /*!local:re2c
        white_space* { CHECK_SYMBOL }
    */
}
#undef CHECK_SYMBOL

/**
 * \brief Skips over any white space characters in the input.
 *
 * This function advances the cursor over any sequence of white space
 * characters, including spaces and tabs, in the given input.
 *
 * \param YYCURSOR Pointer to the current position in the input.
 * \returns A pointer to the position immediately following any skipped
 *          white space characters, or the same position if no white space
 *          was found.
 */
__forceinline char *skip_white_space (char *YYCURSOR) {
    /*!local:re2c
        white_space* { return YYCURSOR; }
    */
}

/**
 * \brief Skips over any white space characters in the input.
 *
 * This function advances the cursor over any sequence of white space
 * characters, including spaces, tabs, carriage returns, and newlines,
 * in the given input.
 *
 * \param YYCURSOR Pointer to the current position in the input.
 * \returns A pointer to the position immediately following any skipped
 *          white space characters, or the same position if no white space
 *          was found.
 */
__forceinline char *skip_any_white_space (char *YYCURSOR) {
    /*!local:re2c
        any_white_space* { return YYCURSOR; }
    */
}

__forceinline char *lex_white_space (char *YYCURSOR) {
    /*!local:re2c
        white_space+ { return YYCURSOR; }
        * { UNEXPECTED_INPUT("expected white space"); }
    */
}

__forceinline char *lex_any_white_space (char *YYCURSOR) {
    /*!local:re2c
        any_white_space+ { return YYCURSOR; }
        * { UNEXPECTED_INPUT("expected any white space"); }
    */
}

__forceinline LexResult<Range> lex_range_argument (char *YYCURSOR) {

    YYCURSOR = skip_white_space(YYCURSOR);
    
    auto firstArgPos = YYCURSOR;
    auto parsed_0 = parse_int<uint32_t>(YYCURSOR);
    YYCURSOR = parsed_0.cursor;
    auto min = parsed_0.value;
    auto max = min;

    YYCURSOR = skip_white_space(YYCURSOR);

    if (*YYCURSOR == '.') {
        YYCURSOR++;
        if (*YYCURSOR != '.') {
            UNEXPECTED_INPUT("expected '..' to mark range");
        }
        
        YYCURSOR = skip_white_space(YYCURSOR + 1);

        auto parsed_1 = parse_int<uint32_t>(YYCURSOR);
        YYCURSOR = parsed_1.cursor;
        max = parsed_1.value;
        if (max <= min) show_input_error("invalid range", firstArgPos, YYCURSOR - 1);
    }

    /*!local:re2c
        white_space* ">" { return { YYCURSOR, {min, max} }; }
        white_space* "," { UNEXPECTED_INPUT("string only accepts one argument"); }
        * { UNEXPECTED_INPUT("expected end of argument list"); }
    */
}

template <KEYWORDS keyword, typename T>
requires std::is_base_of_v<IdentifiedDefinition, T>
__forceinline char *lex_identifier (char *YYCURSOR, IdentifierMap &identifier_map, IdentifiedDefinition* definition, Buffer::Index<T> definition_idx) {
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
    size_t length = end - start;
    if (length > std::numeric_limits<uint16_t>::max()) {
        UNEXPECTED_INPUT("identifier name too long");
    }

    definition->name = {start, static_cast<uint16_t>(length)};
    definition->keyword = keyword;

    char end_backup = *end;
    *end = 0;
    auto inserted = identifier_map.insert({start, Buffer::Index<IdentifiedDefinition>{definition_idx.value}}).second;
    *end = end_backup;

    if (!inserted) {
        show_input_error("identifier already defined", start);
    }

    return YYCURSOR;
}


char *lex_type (char *YYCURSOR, Buffer &buffer, TypeContainer *type_container, IdentifierMap &identifier_map) {
    const char *identifier_start = YYCURSOR;
    #define SIMPLE_TYPE(TYPE) type_container->reserve_type<Type>(buffer)->type = FIELD_TYPE::TYPE; return YYCURSOR;
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

        auto range_result = lex_range_argument(YYCURSOR);
        YYCURSOR = range_result.cursor;

        auto string_type = type_container->reserve_type<StringType>(buffer);

        string_type->type = STRING;
        string_type->range = range_result.value;

        return YYCURSOR;
    }

    array: {
        auto array_type = type_container->reserve_type<ArrayType>(buffer);
        array_type->type = ARRAY;

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        YYCURSOR = skip_white_space(YYCURSOR);

        YYCURSOR = lex_type(YYCURSOR, buffer, array_type, identifier_map);

        YYCURSOR = lex_same_line_symbol<',', "expected length argument">(YYCURSOR);

        auto range_result = lex_range_argument(YYCURSOR);
        YYCURSOR = range_result.cursor;

        array_type->range = range_result.value;

        return YYCURSOR;
    }

    variant: {

        YYCURSOR = lex_same_line_symbol<'<', "expected argument list">(YYCURSOR);

        auto variant_type = type_container->reserve_type<VariantType>(buffer);
        variant_type->type = VARIANT;
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
            show_input_error("identifier not defined", identifier_start - 1);
        }

        auto identified_type = type_container->reserve_type<IdentifiedType>(buffer);
        identified_type->type = IDENTIFIER;
        identified_type->identifier_idx = identifier_idx_iter->second;
        
        return YYCURSOR;
    }
}

template <typename T>
T *skip_type (Type *type) {
    switch (type->type)
    {
    case STRING:
        return (T*)(type->as_string() + 1);
    case IDENTIFIER:
        return (T*)(type->as_identifier() + 1);
    case ARRAY: {
        return skip_type<T>(type->as_array()->inner_type());
    }
    case VARIANT: {
        auto variant_type = type->as_variant();
        auto types_count = variant_type->variant_count;
        Type *type = variant_type->first_variant();
        for (variant_count_t i = 0; i < types_count; i++) {
            type = skip_type<Type>(type);
        }
        return (T*)type;
    }
    default:
        return (T*)(type + 1);
    }
}

__forceinline char *lex_struct_or_union(
    char *YYCURSOR,
    DefinitionWithFields<StructField> *definition,
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
                show_input_error("expected at least one field", YYCURSOR - 1);
            }
            return YYCURSOR;
        }

        name_start:
        char *start = YYCURSOR - 1;
        /*!local:re2c
            [a-zA-Z0-9_]*  { goto name_end; }
        */
        name_end:
        char *end = YYCURSOR;
        size_t length = end - start;
        if (length > std::numeric_limits<uint16_t>::max()) {
            UNEXPECTED_INPUT("field name too long");
        }
        {
            auto field = definition->first_field();
            for (uint32_t i = 0; i < definition->field_count; i++) {
                if (string_section_eq(field->name.offset, field->name.length, start, length)) {
                    show_input_error("field already defined", start);
                }
                field = skip_type<StructField>(field->type());
            }
        }
        /*!local:re2c
            white_space* ":" { goto struct_field; }
            * { UNEXPECTED_INPUT("expected ':'"); }
        */

        struct_field:
        StructField *field = definition->reserve_field(buffer);
        field->name = {start, static_cast<uint16_t>(length)};
        
        YYCURSOR = skip_white_space(YYCURSOR);
        YYCURSOR = lex_type(YYCURSOR, buffer, field, identifier_map);

        field_end:
        YYCURSOR = lex_same_line_symbol<';'>(YYCURSOR);
    }
}

__forceinline auto set_member_value (char *start, uint64_t value, bool is_negative) {
    if (value == std::numeric_limits<uint64_t>::max()) {
        show_input_error("enum member value too large", start);
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

__forceinline auto add_member (EnumDefinition *definition, Buffer &buffer, char *start, char *end, uint64_t value, bool is_negative) {
    auto field = definition->reserve_field(buffer);
    field->name = {start, end};
    field->value = value;
    field->is_negative = is_negative;
}

__forceinline char *lex_enum (
    char *YYCURSOR,
    EnumDefinition *definition,
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
                show_input_error("expected at least one member", YYCURSOR - 1);
            }
            return YYCURSOR;
        }

        name_start:
        char *start = YYCURSOR - 1;
        /*!local:re2c
            [a-zA-Z0-9_]*  { goto name_end; }
        */
        name_end:
        char *end = YYCURSOR;
        size_t length = end - start;
        {
            auto field = definition->first_field();
            for (uint32_t i = 0; i < definition->field_count; i++) {
                if (string_section_eq(field->name.offset, field->name.length, start, length)) {
                    show_input_error("field already defined", start);
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

int printf_with_indent (uint32_t indent, const char *__format, ...) {
    printf("\033[%dC", indent);
    va_list args;
    va_start(args, __format);
    vprintf(__format, args);
    va_end(args);
    return 0;
}



template <typename T>
T *print_type (Type *type, Buffer &buffer, uint32_t indent, std::string prefix = "- type: ") {
    printf("\n");
    printf_with_indent(indent, prefix.c_str());
    switch (type->type)
    {
    case FIELD_TYPE::STRING: {
        auto string_type = type->as_string();
        auto range = string_type->range;
        if (range.min == range.max) {
            printf("string<%d>", range.min);
        } else {
            printf("string<%d..%d>", range.min, range.max);
        }
        return (T*)(string_type + 1);
    }
    case FIELD_TYPE::ARRAY: {
        auto array_type = type->as_array();
        printf("array\n");
        printf_with_indent(indent + 2, "- length: ");
        auto [min, max] = array_type->range;
        if (min == max) {
            printf("%d", min);
        } else {
            printf("%d..%d", min, max);
        }
        return print_type<T>(array_type->inner_type(), buffer, indent + 2, "- inner:");
    }
    case FIELD_TYPE::VARIANT: {
        auto variant_type = type->as_variant();
        // Type **types = variant_type->types;
        auto types_count = variant_type->variant_count;
        Type *type = variant_type->first_variant();
        printf("variant");
        for (variant_count_t i = 0; i < types_count; i++) {
            type = print_type<Type>(type, buffer, indent + 2, "- option: ");
        }
        return (T*)type;
    }
    case FIELD_TYPE::IDENTIFIER: {
        auto identified_type = type->as_identifier();
        auto identifier = buffer.get(identified_type->identifier_idx);
        auto name = identifier->name;
        printf(extract_string(name).c_str());
        return (T*)(identified_type + 1);
    }
    default:
        static const std::string types[] = { "int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64", "float32", "float64", "bool", "string" };
        printf(types[type->type].c_str());
        return (T*)(type + 1);
    }
    
}

void print_parse_result (IdentifierMap &identifier_map, Buffer &buffer) {
    for (auto [name, identifier_idx] : identifier_map) {
        printf("\n\n");
        auto identifier = buffer.get(identifier_idx);
        auto keyword = identifier->keyword;
        switch (keyword) {
            case UNION: {
                printf("union: ");
                goto print_struct_or_union;
            }
            case STRUCT: {
                printf("struct: ");
                print_struct_or_union:
                printf(name.c_str());
                auto struct_definition = identifier->as_struct();
                auto field = struct_definition->first_field();
                for (uint32_t i = 0; i < struct_definition->field_count; i++) {
                    auto name = extract_string(field->name);
                    printf("\n");
                    printf_with_indent(2, "- field: ");
                    printf(name.c_str());
                    field = print_type<StructField>(field->type(), buffer, 4);
                }
                break;
            }

            case ENUM: {
                printf("enum: ");
                printf(name.c_str());
                auto enum_definition = identifier->as_enum();
                EnumField *field = enum_definition->first_field();
                for (uint32_t i = 0; i < enum_definition->field_count; i++) {
                    auto name = extract_string(field->name);
                    printf("\n");
                    printf_with_indent(2, "- memeber: ");
                    printf(name.c_str());
                    printf("\n");
                    printf_with_indent(4, "- value: ");
                    printf(field->is_negative ? "-" : "");
                    printf("%d", field->value);
                    field = field->next();
                }
                break;
            }

            case TYPEDEF: {
                printf("typedef: ");
                printf(name.c_str());
                auto typedef_definition = identifier->as_typedef();
                auto type = typedef_definition->type();
                print_type<Type>(type, buffer, 2);
                break;
            }
        }
    }
}

__forceinline void lex (char *YYCURSOR, IdentifierMap &identifier_map, Buffer &buffer) {
    loop: {
    /*!local:re2c
    
        struct_keyword  = any_white_space* "struct" ;
        enum_keyword    = any_white_space* "enum"   ;
        union_keyword   = any_white_space* "union"  ;
        typedef_keyword = any_white_space* "typedef";

        any_white_space* [\x00] { return; }

        * { UNEXPECTED_INPUT("unexpected input"); }

        struct_keyword  { goto struct_keyword; }
        enum_keyword    { goto enum_keyword; }
        union_keyword   { goto union_keyword; }
        typedef_keyword { goto typedef_keyword; }

    */
    goto loop;
    }

    struct_keyword: {
        auto [definition, definition_idx] = StructDefinition::create(buffer);
        YYCURSOR = lex_identifier<STRUCT>(YYCURSOR, identifier_map, definition, definition_idx);
        YYCURSOR = lex_struct_or_union(YYCURSOR, definition, identifier_map, buffer);
        goto loop;
    }
    enum_keyword: {
        auto [definition, definition_idx] = EnumDefinition::create(buffer);
        YYCURSOR = lex_identifier<ENUM>(YYCURSOR, identifier_map, definition, definition_idx);
        YYCURSOR = lex_enum(YYCURSOR, definition, identifier_map, buffer);
        goto loop;
    }
    union_keyword: {
        auto [definition, definition_idx] = UnionDefinition::create(buffer);
        YYCURSOR = lex_identifier<UNION>(YYCURSOR, identifier_map, definition, definition_idx);
        YYCURSOR = lex_struct_or_union(YYCURSOR, definition, identifier_map, buffer);
        goto loop;
    }
    typedef_keyword: {
        auto [definition, definition_idx] = TypedefDefinition::create(buffer);
        YYCURSOR = skip_white_space(YYCURSOR);
        YYCURSOR = lex_type(YYCURSOR, buffer, definition, identifier_map);
        YYCURSOR = lex_white_space(YYCURSOR);
        YYCURSOR = lex_identifier<TYPEDEF>(YYCURSOR, identifier_map, definition, definition_idx);
        YYCURSOR = lex_same_line_symbol<';', "expected ';'">(YYCURSOR);
        goto loop;
    }
}



#define SIMPLE_ERROR(message) std::cout << "spc.exe: error: " << message << std::endl

int main(int argc, char** argv) {
    if (argc == 1) {
        SIMPLE_ERROR("no input supplied");
        return 1;
    }

    char *path_arg = argv[1];
    if (!std::filesystem::exists(path_arg)) {
        SIMPLE_ERROR("file does not exist");
        return 1;
    }
    auto file_path = std::filesystem::canonical(path_arg);
    file_path_string = file_path.string();
    auto file_size = std::filesystem::file_size(file_path);

    FILE* file = fopen(path_arg, "rb");
    if (!file) {
        SIMPLE_ERROR("could not open file");
        return 1;
    }
    char data[file_size + 1] = {0};
    if (fread(data, 1, file_size, file) != file_size) {
        SIMPLE_ERROR("could not read file");
        return 1;
    }
    fclose(file);
    
    std::cout << "Lexing input of length: " << std::setprecision(0) << file_size << std::endl;
    input_start = data;

    auto start_ts = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < 5000000; i++)
    {
        IdentifierMap identifier_map;
        uint8_t __buffer[5000];
        auto buffer = Buffer(__buffer);
        lex(data, identifier_map, buffer);
        print_parse_result(identifier_map, buffer);
        buffer.free();
    }

    auto end_ts = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts);
    std::cout << "Time taken: " << duration.count() << " milliseconds" << std::endl;
    

}
