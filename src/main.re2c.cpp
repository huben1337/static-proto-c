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
#include "internal_error.cpp"
#include "memory.cpp"
#include "lexer.re2c.cpp"



template <typename T>
INLINE T* add_offset(auto* ptr, size_t offset) {
    return reinterpret_cast<T*>(reinterpret_cast<size_t>(ptr) + offset);
}

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
T* print_type (Type* type, Buffer &buffer, uint32_t indent, std::string prefix = "- type: ") {
    printf("\n");
    printf_with_indent(indent, prefix.c_str());
    switch (type->type)
    {
    case FIELD_TYPE::FIXED_STRING: {
        auto fixed_string_type = type->as_fixed_string();
        printf("string<%d..%d>", fixed_string_type->length);
        return reinterpret_cast<T*>(fixed_string_type + 1);
    }
    case FIELD_TYPE::STRING: {
        auto string_type = type->as_string();
        auto [min, max] = string_type->range;
        printf("string<%d..%d>", min, max);
        return reinterpret_cast<T*>(string_type + 1);
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
        auto types_count = variant_type->variant_count;
        Type* type = variant_type->first_variant();
        printf("variant");
        for (variant_count_t i = 0; i < types_count - 1; i++) {
            type = print_type<Type>(type, buffer, indent + 2, "- option: ");
        }
        return print_type<T>(type, buffer, indent + 2, "- option: ");
    }
    case FIELD_TYPE::IDENTIFIER: {
        auto identified_type = type->as_identifier();
        auto identifier = buffer.get(identified_type->identifier_idx);
        auto name = identifier->data()->name;
        printf(extract_string(name).c_str());
        return reinterpret_cast<T*>(identified_type + 1);
    }
    default:
        static const std::string types[] = { "int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64", "float32", "float64", "bool", "string" };
        printf(types[type->type].c_str());
        return reinterpret_cast<T*>(type + 1);
    }
    
}

void print_parse_result (IdentifierMap &identifier_map, Buffer &buffer) {
    for (auto [name, identifier_idx] : identifier_map) {
        printf("\n\n");
        auto identifier = buffer.get(identifier_idx);
        auto keyword = identifier->keyword;
        auto definition_data = identifier->data();
        switch (keyword) {
            case UNION: {
                printf("union: ");
                goto print_struct_or_union;
            }
            case STRUCT: {
                printf("struct: ");
                print_struct_or_union:
                printf(name.c_str());
                auto struct_definition = definition_data->as_struct();
                auto field = struct_definition->first_field()->data();
                for (uint32_t i = 0; i < struct_definition->field_count - 1; i++) {
                    auto name = extract_string(field->name);
                    printf("\n");
                    printf_with_indent(2, "- field: ");
                    printf(name.c_str());
                    field = print_type<StructField>(field->type(), buffer, 4)->data();
                }
                print_type<StructField>(field->type(), buffer, 4);
                break;
            }

            case ENUM: {
                printf("enum: ");
                printf(name.c_str());
                auto enum_definition = definition_data->as_enum();
                EnumField* field = enum_definition->first_field();
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

constexpr size_t get_type_size (FIELD_TYPE type) {
    switch (type) {
        case INT8:      return 8;
        case INT16:     return 16;
        case INT32:     return 32;
        case INT64:     return 64;
        case UINT8:     return 8;
        case UINT16:    return 16;
        case UINT32:    return 32;
        case UINT64:    return 64;
        case FLOAT32:   return 32;
        case FLOAT64:   return 64;
        case BOOL:      return 1;
        default:        return 0;
    }
}


template <size_t leaf_size>
void __extract_leafes_def (KEYWORDS keyword, IdentifiedDefinition::Data* definition_data, Buffer &buffer, std::string path);

template <size_t leaf_size, typename T>
T* __extract_leafes_type (Type* type, Buffer &buffer, std::string path) {
    switch (type->type)
    {
    case FIELD_TYPE::FIXED_STRING: {
        if constexpr (leaf_size == 8) {
            printf("%s\n", path.c_str());
        }
        return (T*)(type->as_fixed_string() + 1);
    }
    case FIELD_TYPE::STRING: {
        return (T*)(type->as_string() + 1);
    }
    case FIELD_TYPE::ARRAY: {
        auto array_type = type->as_array();
        auto [min, max] = array_type->range;
        if (min == max) {
            return __extract_leafes_type<leaf_size, T>(array_type->inner_type(), buffer, path + "[" + std::to_string(min) + "]");
        } else {
            return skip_type<T>(array_type->inner_type());
        }
    }
    case FIELD_TYPE::VARIANT: {
        INTERNAL_ERROR("not implemented");
    }
    case FIELD_TYPE::IDENTIFIER: {
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
void __extract_leafes_def (KEYWORDS keyword, IdentifiedDefinition::Data* definition_data, Buffer &buffer, std::string path) {
    switch (keyword)
        {
        case UNION:
            INTERNAL_ERROR("union not implemented");
            break;
        case ENUM:
            INTERNAL_ERROR("enum not implemented");
            break;
        /* case TYPEDEF: {
            auto typedef_definition = definition_data->as_typedef();
            auto type = typedef_definition->type();
            __extract_leafes_type<leaf_size, Type>(type, buffer, path);
            break;
        } */
        case STRUCT: {
            auto struct_definition = definition_data->as_struct();
            StructField::Data* field = struct_definition->first_field()->data();
            for (uint32_t i = 0; i < struct_definition->field_count - 1; i++) {
                auto name = extract_string(field->name);
                field = __extract_leafes_type<leaf_size, StructField>(field->type(), buffer, path + "." + name)->data();
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
    if (argc == 1) {
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

    for (size_t i = 0; i < 1; i++)
    {
        IdentifierMap identifier_map;
        uint8_t __buffer[5000];
        auto buffer = Buffer(__buffer);
        auto target = lex(data, identifier_map, buffer);
        auto target_data = target->data();
        auto target_name = extract_string(target_data->name);
        for_<size_t, 64, 32, 16, 8>([&target, &target_data, &buffer, &target_name]<size_t v>() {
            printf("size: %d\n", v);
            __extract_leafes_def<v>(target->keyword, target_data, buffer, target_name);
        });

        uint8_t __buffer2[5000];
        auto buffer2 = Buffer(__buffer2);

        
        // #define DO_PRINT
        #ifdef DO_PRINT
        // printf("\n\n- target: ");
        // printf(extract_string(target->as_struct()->name).c_str());
        print_parse_result(identifier_map, buffer);
        #endif
        buffer.free();
    }

    auto end_ts = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts);
    std::cout << "\n\nTime taken: " << duration.count() << " milliseconds" << std::endl;
}
