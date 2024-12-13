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
#include <array>

#include "base.cpp"
#include "fatal_error.cpp"
#include "memory.cpp"
#include "lexer.re2c.cpp"
#include "codegen.cpp"


const char* input_start;
std::string file_path_string;

int printf_with_indent (uint32_t indent, const char* __format, ...) {
    printf("\033[%dC", indent);
    va_list args;
    va_start(args, __format);
    vprintf(__format, args);
    va_end(args);
    return 0;
}

template <typename T>
T* print_type (lexer::Type* type, Buffer &buffer, uint32_t indent, std::string prefix = "- type: ") {
    printf("\n");
    printf_with_indent(indent, prefix.c_str());
    switch (type->type)
    {
    case lexer::FIELD_TYPE::STRING_FIXED: {
        auto fixed_string_type = type->as_fixed_string();
        printf("string\n");
        printf_with_indent(indent + 2, "- length: %d", fixed_string_type->length);
        return reinterpret_cast<T*>(fixed_string_type + 1);
    }
    case lexer::FIELD_TYPE::STRING: {
        auto string_type = type->as_string();
        printf("string\n");
        printf_with_indent(indent + 2, "- min length: %d\n", string_type->min_length);
        printf_with_indent(indent + 2, "- length data size: %d", string_type->fixed_alignment);
        return reinterpret_cast<T*>(string_type + 1);
    }
    case lexer::FIELD_TYPE::ARRAY_FIXED: {
        auto array_type = type->as_array();
        printf("static array\n");
        printf_with_indent(indent + 2, "- length: %d", array_type->length);
        return print_type<T>(array_type->inner_type(), buffer, indent + 2, "- inner: ");
    }
    case lexer::FIELD_TYPE::ARRAY: {
        auto array_type = type->as_array();
        printf("dynamic array\n");
        printf_with_indent(indent + 2, "- min length: %d\n", array_type->length);
        printf_with_indent(indent + 2, "- length data size: %d", array_type->fixed_alignment);
        return print_type<T>(array_type->inner_type(), buffer, indent + 2, "- inner: ");
    }
    case lexer::FIELD_TYPE::VARIANT: {
        auto variant_type = type->as_variant();
        auto types_count = variant_type->variant_count;
        auto type = variant_type->first_variant();
        printf("variant");
        for (lexer::variant_count_t i = 0; i < types_count - 1; i++) {
            type = print_type<lexer::Type>(type, buffer, indent + 2, "- option: ");
        }
        return print_type<T>(type, buffer, indent + 2, "- option: ");
    }
    case lexer::FIELD_TYPE::IDENTIFIER: {
        auto identified_type = type->as_identifier();
        auto identifier = buffer.get(identified_type->identifier_idx);
        auto name = identifier->data()->name;
        printf(extract_string(name).c_str());
        return reinterpret_cast<T*>(identified_type + 1);
    }
    default:
        static const std::string types[] = { "bool", "uint8", "uint16", "uint32", "uint64", "int8", "int16", "int32", "int64", "float32", "float64" };
        printf(types[type->type].c_str());
        return reinterpret_cast<T*>(type + 1);
    }
    
}

void print_parse_result (lexer::IdentifierMap &identifier_map, Buffer &buffer) {
    for (auto [name, identifier_idx] : identifier_map) {
        printf("\n\n");
        auto identifier = buffer.get(identifier_idx);
        auto keyword = identifier->keyword;
        auto definition_data = identifier->data();
        switch (keyword) {
            case lexer::KEYWORDS::UNION: {
                printf("union: ");
                goto print_struct_or_union;
            }
            case lexer::KEYWORDS::STRUCT: {
                printf("struct: ");
                print_struct_or_union:
                printf(name.c_str());
                auto struct_definition = definition_data->as_struct();
                printf(struct_definition->is_fixed_size ? " (fixed size)" : " (dynamic size)");
                auto leaf_counts = struct_definition->leaf_counts;
                printf(" leafes: { %d, %d, %d, %d }", leaf_counts.size8, leaf_counts.size16, leaf_counts.size32, leaf_counts.size64);
                auto field = struct_definition->first_field();
                for (uint32_t i = 0; i < struct_definition->field_count; i++) {
                    auto field_data = field->data();
                    auto name = extract_string(field_data->name);
                    printf("\n");
                    printf_with_indent(2, "- field: ");
                    printf(name.c_str());
                    field = print_type<lexer::StructField>(field_data->type(), buffer, 4);
                }
                break;
            }

            case lexer::KEYWORDS::ENUM: {
                printf("enum: %s\n", name.c_str());
                printf_with_indent(2, "- type size: %d bytes", definition_data->as_enum()->type_size);
                auto enum_definition = definition_data->as_enum();
                auto field = enum_definition->first_field();
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

            /* case TYPEDEF: {
                printf("typedef: ");
                printf(name.c_str());
                auto typedef_definition = definition_data->as_typedef();
                auto type = typedef_definition->type();
                print_type<Type>(type, buffer, 2);
                break;
            } */
        }
    }
}

constexpr size_t get_type_size (lexer::FIELD_TYPE type) {
    switch (type) {
        case lexer::INT8:       return 8;
        case lexer::INT16:      return 16;
        case lexer::INT32:      return 32;
        case lexer::INT64:      return 64;
        case lexer::UINT8:      return 8;
        case lexer::UINT16:     return 16;
        case lexer::UINT32:     return 32;
        case lexer::UINT64:     return 64;
        case lexer::FLOAT32:    return 32;
        case lexer::FLOAT64:    return 64;
        case lexer::BOOL:       return 1;
        default:                return 0;
    }
}


template <size_t leaf_size>
void __extract_leafes_def (lexer::KEYWORDS keyword, lexer::IdentifiedDefinition::Data* definition_data, Buffer &buffer, std::string path);

template <size_t leaf_size, typename T>
T* __extract_leafes_type (lexer::Type* type, Buffer &buffer, std::string path) {
    switch (type->type)
    {
    case lexer::FIELD_TYPE::STRING_FIXED: {
        if constexpr (leaf_size == 8) {
            printf("%s\n", path.c_str());
        }
        return (T*)(type->as_fixed_string() + 1);
    }
    case lexer::FIELD_TYPE::STRING: {
        auto string_type = type->as_string();
        if (string_type->fixed_alignment * 8 == leaf_size) {
            printf("%s\n", (path + " (hidden)").c_str());
        }
        return (T*)(string_type + 1);
    }
    case lexer::FIELD_TYPE::ARRAY_FIXED: {
        auto array_type = type->as_array();
        return __extract_leafes_type<leaf_size, T>(array_type->inner_type(), buffer, path + "[" + std::to_string(array_type->length) + "]");
    }
    case lexer::FIELD_TYPE::ARRAY: {
        auto array_type = type->as_array();
        if (array_type->fixed_alignment * 8 == leaf_size) {
            printf("%s\n", (path + " (hidden)").c_str());
        }
        return skip_type<T>(array_type->inner_type());
    }
    case lexer::FIELD_TYPE::VARIANT: {
        INTERNAL_ERROR("not implemented");
    }
    case lexer::FIELD_TYPE::IDENTIFIER: {
        auto identified_type = type->as_identifier();
        auto identifier = buffer.get(identified_type->identifier_idx);
        auto identifier_data = identifier->data();
        __extract_leafes_def<leaf_size>(identifier->keyword, identifier_data, buffer, path + "->" + extract_string(identifier_data->name));
        return (T*)(identified_type + 1);
    }
    default:
        if (get_type_size(type->type) == leaf_size) {
            printf("%s\n", path.c_str());
        }
        return (T*)(type + 1);
    }
}

template <size_t leaf_size>
void __extract_leafes_def (lexer::KEYWORDS keyword, lexer::IdentifiedDefinition::Data* definition_data, Buffer &buffer, std::string path) {
    switch (keyword)
        {
        case lexer::KEYWORDS::UNION:
            INTERNAL_ERROR("union not implemented");
            break;
        case lexer::KEYWORDS::ENUM:
            INTERNAL_ERROR("enum not implemented");
            break;
        /* case TYPEDEF: {
            auto typedef_definition = definition_data->as_typedef();
            auto type = typedef_definition->type();
            __extract_leafes_type<leaf_size, Type>(type, buffer, path);
            break;
        } */
        case lexer::KEYWORDS::STRUCT: {
            auto struct_definition = definition_data->as_struct();
            auto field = struct_definition->first_field()->data();
            for (uint32_t i = 0; i < struct_definition->field_count - 1; i++) {
                auto name = extract_string(field->name);
                field = __extract_leafes_type<leaf_size, lexer::StructField>(field->type(), buffer, path + "." + name)->data();
            }
            auto name = extract_string(field->name);
            __extract_leafes_type<leaf_size, void>(field->type(), buffer, path + "." + name);
        }
        
    }
}


template <typename F, typename T, T Value>
concept CallableWithValue = requires(F func) {
    func.template operator()<Value>();
};

template <typename T, T... Values, typename F>
requires (CallableWithValue<F, T, Values> && ...)
void for_(F&& func) {
    (func.template operator()<Values>(), ...);
}

#define SIMPLE_ERROR(message) std::cout << "spc.exe: error: " << message << std::endl

int main(int argc, const char** argv) {
    std::cout << "spc.exe" << std::endl;
    if (argc <= 1) {
        SIMPLE_ERROR("no input supplied");
        return 1;
    }

    auto path_arg = argv[1];
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
    char data[file_size + 1];
    data[file_size] = 0;
    if (fread(data, 1, file_size, file) != file_size) {
        SIMPLE_ERROR("could not read file");
        return 1;
    }
    fclose(file);
    
    std::cout << "Lexing input of length: " << std::setprecision(0) << file_size << std::endl;
    input_start = data;

    auto start_ts = std::chrono::high_resolution_clock::now();

    codegen::test();

    //#define DO_LEX
    #ifdef DO_LEX
    for (size_t i = 0; i < 5000000; i++)
    {
        lexer::IdentifierMap identifier_map;
        uint8_t __buffer[5000];
        auto buffer = Buffer(__buffer);
        auto target = lexer::lex(data, identifier_map, buffer);
        auto target_data = target->data();
        // auto target_name = extract_string(target_data->name);

        //#define DO_EXTRACT
        #ifdef DO_EXTRACT
        for_<size_t, 64, 32, 16, 8>([&target, &target_data, &buffer, &target_name]<size_t v>() {
            printf("size: %d\n", v);
            __extract_leafes_def<v>(target->keyword, target_data, buffer, target_name);
        });
        #endif
        // #define DO_PRINT
        #ifdef DO_PRINT
        // printf("\n\n- target: ");
        // printf(extract_string(target->as_struct()->name).c_str());
        print_parse_result(identifier_map, buffer);
        #endif

        uint8_t __buffer2[target_data->internal_size + 8];
        auto buffer2 = Buffer(__buffer2, target_data->internal_size);

        // parser::StructFieldWithOffset size8_leafes[target_data->leaf_counts.size8];
        // parser::StructFieldWithOffset size16_leafes[target_data->leaf_counts.size16];
        // parser::StructFieldWithOffset size32_leafes[target_data->leaf_counts.size32];
        // parser::StructFieldWithOffset size64_leafes[target_data->leaf_counts.size64];

        // printf("\ntarget internal size: %d\n", target_data->internal_size);
        // printf("buffer position: %d\n", buffer.current_position());
        buffer.free();
    }
    #endif

    auto end_ts = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts);
    std::cout << "\n\nTime taken: " << duration.count() << " milliseconds" << std::endl;
}
