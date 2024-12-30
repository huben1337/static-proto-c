#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <array>
#include "base.cpp"
#include "codegen.cpp"
#include "string_helpers.cpp"
#include "lexer_types.cpp"

namespace decode_code {

struct FixedFieldLeafes {
    uint32_t position;
    uint32_t capacity;
    struct Leaf {
        Buffer::Index<char> offset_str_idx;
        uint64_t size;
    }* leafes;

    INLINE auto reserve_next () {
        if (position == capacity) {
            printf("capacity exceeded\n");
            exit(1);
        }
        return leafes + position++;
    };
};

template <typename F, typename T, T Value>
concept CallableWithValue = requires(F func) {
    func.template operator()<Value>();
};
template <typename T, T... Values, typename F>
requires (CallableWithValue<F, T, Values> && ...)
void for_(F&& func) {
    (func.template operator()<Values>(), ...);
}

template <typename T, T... Values, typename F>
requires (CallableWithValue<F, T, Values> && ...)
void for_(F&& func, const std::integer_sequence<T, Values...>) {
    (func.template operator()<Values>(), ...);
}




const char* input_start;
std::string file_path_string;



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
    std::string ctro_args;
    std::string ctor_inits;
    std::string ctor_used;
};

template <uint8_t array_depth>
INLINE ArrayCtorStrs make_array_ctor_strs () {
    std::string ctro_args = "size_t __base__";
    std::string ctor_inits = "__base__(__base__)";
    std::string ctor_used = "return {__base__";

    for_([&]<uint32_t v>() {
        std::string idx_str = "idx_" + std::to_string(v);
        ctro_args += ", uint32_t " + idx_str;
        ctor_inits += ", " + idx_str + "(" + idx_str + ")";
        ctor_used += ", " + idx_str;
    }, std::make_integer_sequence<uint32_t, array_depth>{});

    ctor_used += "};";

    return {ctro_args, ctor_inits, ctor_used};
}

template <size_t array_depth>
requires (array_depth > 0)
INLINE std::string make_idx_calc (std::array<uint32_t, array_depth>& array_lengths) {
    std::string idx_calc = "";
    for_([&]<uint32_t v>() {
        if constexpr (v < array_depth - 1) {
            uint64_t size = 1;
            for_([&]<uint32_t i>() {
                size *= array_lengths[(array_depth - 1) - i + v];
            }, std::make_integer_sequence<uint32_t, (array_depth - 1) - v>{});
            if constexpr (v > 0) {
                idx_calc += " + idx_" + std::to_string(v) + " * " + std::to_string(size);
            } else {
                idx_calc += "idx_" + std::to_string(v) + " * " + std::to_string(size);
            }
        } else {
            if constexpr (v > 0) {
                idx_calc += " + idx_" + std::to_string(v);
            } else {
                idx_calc += "idx_" + std::to_string(v);
            }
        }
    }, std::make_integer_sequence<uint32_t, array_depth>{});

    return idx_calc;
}

template <typename T, size_t indent, typename Derived>
struct GenStructFieldInArrayResult {
    T* next;
    Derived code;
    uint64_t inner_length;
};

template <typename T, size_t indent, typename Derived>
struct GenStructFieldResult {
    T* next;
    Derived code;
};

// template <typename T, uint8_t depth, uint8_t array_depth, size_t indent, typename Last>
// GenStructFieldInArrayResult<T, indent, codegen::NestedStruct<indent, Last>> gen_struct_field (lexer::StructField::Data* field, Buffer &buffer, codegen::NestedStruct<indent, Last> code, FixedFieldLeafes& leafes) {

template <typename T, uint8_t depth, uint8_t array_depth, size_t indent, typename Last, typename Derived>
GenStructFieldResult<T, indent, Derived> gen_struct_field (lexer::StructField::Data* field, Buffer &buffer, codegen::__Struct<indent, Last, Derived> code, FixedFieldLeafes& leafes, uint64_t outer_array_length, std::array<uint32_t, array_depth>& array_lengths) {
    if constexpr (indent >= 6) {
        INTERNAL_ERROR("indent too large");
    } else {
    auto field_type = field->type();
    auto name = extract_string(field->name);
    auto unique_name = name + "_" + std::to_string(depth);
    switch (field_type->type)
    {
    case lexer::FIELD_TYPE::STRING_FIXED: {
        auto fixed_string_type = field_type->as_fixed_string();
        uint32_t length = fixed_string_type->length;
        auto size_type_str =  get_size_type_str(length);
        auto leaf = leafes.reserve_next();
        leaf->size = length * outer_array_length;

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs<array_depth>();

        auto string_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);
        
        if constexpr (array_depth == 0) {
            string_struct = string_struct
            .method("const char* c_str")
                ("return add_offset<const char*>(__base__, ")("00000000);", &leaf->offset_str_idx).nl()
            .end();
        } else {
            string_struct = string_struct
            .method("const char* c_str")
                ("return add_offset<const char*>(__base__, ")("00000000 + ", &leaf->offset_str_idx)(make_idx_calc(array_lengths))(length)(");").nl()
            .end();
        }

        string_struct = string_struct
            .method("constexpr " + size_type_str + " size")
                ("return ")(length)(";").nl()
            .end()
            .method("constexpr " + size_type_str + " length")
                .line("return size() - 1;")
            .end()
            ._private()
            .field("size_t", "__base__");

        for (uint32_t i = 0; i < array_depth; i++) {
            string_struct = string_struct
            .field("uint32_t", "idx_" + std::to_string(i));
        }
        
        code = string_struct
        .end()
        .method(unique_name + " " + name)
            .line(array_ctor_strs.ctor_used)
        .end();
        return {(T*)(fixed_string_type + 1), code};
    }
    case lexer::FIELD_TYPE::ARRAY_FIXED: {
        auto array_type = field_type->as_array();
        uint32_t length = array_type->length;
        auto size_type_str = get_size_type_str(length);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs<array_depth>();

        auto array_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);

        auto new_array_lengths = std::array<uint32_t, array_depth + 1>();
        for (uint32_t i = 0; i < array_depth; i++) {
            new_array_lengths[i] = array_lengths[i];
        }
        new_array_lengths[array_depth] = length;

        auto result = gen_array_element<T, array_depth + 1, 0>(array_type->inner_type(), buffer, array_struct, leafes, array_type->length * outer_array_length, new_array_lengths);

        array_struct  = result.code
            .method("constexpr " + size_type_str + " length")
                ("return ")(length)(";").nl()
            .end()
            ._private()
            .field("size_t", "__base__");

        for (uint32_t i = 0; i < array_depth; i++) {
            array_struct = array_struct
            .field("uint32_t", "idx_" + std::to_string(i));
        }

        code = array_struct
        .end()
        .method(unique_name + " " + name)
            .line(array_ctor_strs.ctor_used)
        .end();

        return {result.next, code};
    }
    case lexer::FIELD_TYPE::ARRAY: {
        auto array_type = field_type->as_array();
        uint32_t min_length = array_type->length;
        lexer::SIZE size_size = array_type->size_size;
        auto size_type_str = get_size_type_str(size_size);

        auto leaf = leafes.reserve_next();
        leaf->size = size_size;

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs<array_depth>();

        auto array_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);

        auto new_array_lengths = std::array<uint32_t, array_depth + 1>();
        for (uint32_t i = 0; i < array_depth; i++) {
            new_array_lengths[i] = array_lengths[i];
        }
        new_array_lengths[array_depth] = min_length;

        auto next = lexer::skip_type<T>(array_type->inner_type());

        array_struct = array_struct
            .method(size_type_str + " length")
                ("return *add_offset<")(size_type_str)(">(__base__, ")("00000000);", &leaf->offset_str_idx).nl()
            .end()
            ._private()
            .field("size_t", "__base__");

        for (uint32_t i = 0; i < array_depth; i++) {
            array_struct = array_struct
            .field("uint32_t", "idx_" + std::to_string(i));
        }

        code = array_struct
        .end()
        .method(unique_name + " " + name)
            .line(array_ctor_strs.ctor_used)
        .end();

        return {next, code};
    }
    case lexer::FIELD_TYPE::IDENTIFIER: {
        auto identified_type = field_type->as_identifier();
        auto identifier = buffer.get(identified_type->identifier_idx);
        if (identifier->keyword != lexer::KEYWORDS::STRUCT) {
            INTERNAL_ERROR("not implemented");
        }

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs<array_depth>();

        auto struct_type = identifier->data()->as_struct();
        auto struct_code = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);

        auto field = struct_type->first_field();
        for (uint32_t i = 0; i < struct_type->field_count; i++) {
            auto field_data = field->data();
            auto result = gen_struct_field<lexer::StructField, depth + 1, array_depth>(field_data, buffer, struct_code, leafes, outer_array_length, array_lengths);
            field = result.next;
            struct_code = c_cast<decltype(struct_code)>(result.code);
        }

        struct_code = struct_code
            ._private()
            .field("size_t", "__base__");
        
        for (uint32_t i = 0; i < array_depth; i++) {
            struct_code = struct_code
            .field("uint32_t", "idx_" + std::to_string(i));
        }

        code = struct_code
        .end()
        .method(unique_name + " " + name)
            .line(array_ctor_strs.ctor_used)
        .end();

        return {(T*)(identified_type + 1), code};
    }
    default: {
        static const std::string types[] = { "bool", "uint8_t", "uint16_t", "uint32_t", "uint64_T", "int8_t", "int16_t", "int32_t", "int64_t", "float32_t", "float64_t" };
        static const std::string type_size_strs[] = { "1", "1", "2", "4", "8", "1", "2", "4", "8", "4", "8" };
        static const uint64_t type_sizes[] = { 1, 1, 2, 4, 8, 1, 2, 4, 8, 4, 8 };
        std::string type_name = types[field_type->type];
        auto leaf = leafes.reserve_next();
        leaf->size = type_sizes[field_type->type] * outer_array_length;
        if constexpr (array_depth == 0) {
            code = code
            .method(type_name + " " + name)
                ("return *add_offset<")(type_name)(">(__base__, ")("00000000);", &leaf->offset_str_idx).nl()
            .end();
        } else {
            code = code
            .method(type_name + " " + name)
                ("return *add_offset<")(type_name)(">(__base__, ")("00000000 + (", &leaf->offset_str_idx)(make_idx_calc(array_lengths))(") *")(type_size_strs[field_type->type])(");").nl()
            .end();
        }
        return {(T*)(field_type + 1), code};
    }
    }
    }
}

template <typename T, uint8_t array_depth, uint8_t element_depth, size_t indent, typename Last>
GenStructFieldInArrayResult<T, indent, codegen::NestedStruct<indent, Last>> gen_array_element (lexer::Type* inner_type, Buffer &buffer, codegen::NestedStruct<indent, Last> array_struct, FixedFieldLeafes& leafes, uint64_t outer_array_length, std::array<uint32_t, array_depth>& array_lengths) {
    if constexpr (indent >= 6) {
        INTERNAL_ERROR("indent too large");
    } else {
    switch (inner_type->type)
    {
    case lexer::FIELD_TYPE::STRING_FIXED: {
        auto fixed_string_type = inner_type->as_fixed_string();
        uint32_t length = fixed_string_type->length;
        auto size_type_str =  get_size_type_str(length);
        // std::string element_struct_name = "String_" + array_field_name;
        auto leaf = leafes.reserve_next();
        leaf->size = length * outer_array_length;

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs<array_depth>();

        auto string_struct = array_struct
        ._struct("String")
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits)
            .method("const char* c_str")
                ("return reinterpret_cast<const char*>(__base__ + ")("00000000 + (", &leaf->offset_str_idx)(make_idx_calc(array_lengths))(") *")(length)(");").nl()
            .end()
            .method("constexpr " + size_type_str + " size")
                ("return ")(length)(";").nl()
            .end()
            .method("constexpr " + size_type_str + " length")
                .line("return size() - 1;")
            .end()
            ._private()
            .field("size_t", "__base__");

        for (uint32_t i = 0; i < array_depth; i++) {
            string_struct = string_struct
            .field("uint32_t", "idx_" + std::to_string(i));
        }

        array_struct = string_struct
        .end()
        .method("String get", "uint32_t idx_" + std::to_string(array_depth - 1))
            .line(array_ctor_strs.ctor_used)
        .end();

        return {(T*)(fixed_string_type + 1), array_struct};
    }
    case lexer::FIELD_TYPE::ARRAY_FIXED: {
        auto array_type = inner_type->as_array();
        uint32_t length = array_type->length;
        auto size_type_str = get_size_type_str(length);
        std::string element_struct_name = "Element_" + std::to_string(element_depth);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs<array_depth>();

        auto sub_array_struct = array_struct
        ._struct(element_struct_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);

        auto new_array_lengths = std::array<uint32_t, array_depth + 1>();
        for (uint32_t i = 0; i < array_depth; i++) {
            new_array_lengths[i] = array_lengths[i];
        }
        new_array_lengths[array_depth] = length;

        auto result = gen_array_element<T, array_depth + 1, element_depth + 1>(array_type->inner_type(), buffer, sub_array_struct, leafes, array_type->length * outer_array_length, new_array_lengths);

        sub_array_struct = result.code
            .method("constexpr " + size_type_str + " length")
                ("return ")(length)(";").nl()
            .end()
            ._private()
            .field("size_t", "__base__");

        for (uint32_t i = 0; i < array_depth; i++) {
            sub_array_struct = sub_array_struct
            .field("uint32_t", "idx_" + std::to_string(i));
        }

        array_struct = sub_array_struct
        .end()
        .method(element_struct_name + " get", "uint32_t idx_" + std::to_string(array_depth - 1))
            .line(array_ctor_strs.ctor_used)
        .end();

        return {result.next, array_struct};
    }
    case lexer::FIELD_TYPE::VARIANT: {
        INTERNAL_ERROR("not implemented");
    }
    case lexer::FIELD_TYPE::IDENTIFIER: {
        auto identified_type = inner_type->as_identifier();
        auto identifier = buffer.get(identified_type->identifier_idx);
        if (identifier->keyword != lexer::KEYWORDS::STRUCT) {
            INTERNAL_ERROR("not implemented");
        }
        auto struct_type = identifier->data()->as_struct();
        std::string element_struct_name = extract_string(struct_type->name);
        
        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs<array_depth>();

        auto element_struct = array_struct
        ._struct(element_struct_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);

        auto field = struct_type->first_field();
        for (uint32_t i = 0; i < struct_type->field_count; i++) {
            auto field_data = field->data();
            auto result = gen_struct_field<lexer::StructField, 0, array_depth>(field_data, buffer , element_struct, leafes, outer_array_length, array_lengths);
            field = result.next;
            element_struct = c_cast<decltype(element_struct)>(result.code);
        }

        element_struct = element_struct
            ._private()
            .field("size_t", "__base__");

        for (uint32_t i = 0; i < array_depth; i++) {
            element_struct = element_struct
            .field("uint32_t", "idx_" + std::to_string(i));
        }

        array_struct = element_struct
        .end()
        .method(element_struct_name + " get", "uint32_t idx_" + std::to_string(array_depth - 1))
            .line(array_ctor_strs.ctor_used)
        .end();

        return {(T*)(identified_type + 1), array_struct};
    }
    default: {
        static const std::string types[] = { "bool", "uint8_t", "uint16_t", "uint32_t", "uint64_T", "int8_t", "int16_t", "int32_t", "int64_t", "float32_t", "float64_t" };
        static const std::string size_type_strs[] = { "1", "1", "2", "4", "8", "1", "2", "4", "8", "4", "8" };
        static const uint64_t type_sizes[] = { 1, 1, 2, 4, 8, 1, 2, 4, 8, 4, 8 };
        auto leaf = leafes.reserve_next();
        leaf->size = type_sizes[inner_type->type] * outer_array_length;
        std::string type_name = types[inner_type->type];
        if constexpr (array_depth == 0) {
            array_struct = array_struct
            .method(type_name + " get", "uint32_t index")
                ("return *add_offset<")(type_name)(">(__base__, ")("00000000 + index *", &leaf->offset_str_idx)(size_type_strs[inner_type->type])(");").nl()
            .end();
        } else {
            array_struct = array_struct
            .method(type_name + " get", "uint32_t idx_" + std::to_string(array_depth - 1))
                ("return *add_offset<")(type_name)(">(__base__, ")("00000000 + (", &leaf->offset_str_idx)(make_idx_calc(array_lengths))(") * ")(size_type_strs[inner_type->type])(");").nl()
            .end();
        }
        return {(T*)(inner_type + 1), array_struct};
    }
    }
    }
}

void generate (lexer::StructDefinition* target_struct, Buffer& buffer) {
    uint8_t _buffer[5000];
    auto code = codegen::create_code(_buffer);
    auto total_fixed_leafes = target_struct->leaf_counts.total();
    FixedFieldLeafes::Leaf __leafes[total_fixed_leafes];
    FixedFieldLeafes leafes = { 0, total_fixed_leafes, __leafes };
    std::string struct_name = extract_string(target_struct->name);
    auto struct_code = code
    ._struct(struct_name)
        .ctor("size_t __base__", "__base__(__base__)");

    auto field = target_struct->first_field();
    for (uint32_t i = 0; i < target_struct->field_count; i++) {
        auto field_data = field->data();
        auto s = std::array<uint32_t, 0>{};
        auto result = gen_struct_field<lexer::StructField, 0, 0>(field_data, buffer, struct_code, leafes, 1, s);
        field = result.next;
        struct_code = c_cast<decltype(struct_code)>(result.code);
    }

    auto code_done = struct_code
        ._private()
        .field("size_t", "__base__")
    .end()
    .end();
    uint32_t offset = 0;
    for (uint32_t i = 0; i < total_fixed_leafes; i++) {
        auto leaf = leafes.leafes[i];
        auto offset_str = code_done.buffer().get(leaf.offset_str_idx);
        uint32_t v = offset;
        uint8_t p = 7;
        while (v > 0) {
            offset_str[p--] = '0' + (v % 10);
            v /= 10;
        }
        offset += leaf.size;

    }
    printf("%s\n", code_done.c_str());
    code_done.dispose();
}
}