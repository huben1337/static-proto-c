#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <sys/stat.h>
#include <fcntl.h>
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
#include "decode_code.cpp"
#include "constexpr_helpers.cpp"


const char* input_start;
const char* input_file_path;

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
        printf_with_indent(indent + 2, "- length data size: %d", string_type->stored_size_size);
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
        printf_with_indent(indent + 2, "- length data size: %d", array_type->stored_size_size);
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
                printf(struct_definition->var_leafs_count.as_uint64 == 0 ? " (fixed size)" : " (dynamic size)");
                auto leaf_counts = struct_definition->fixed_leaf_counts.counts;
                printf(" leafs: { %d, %d, %d, %d }", leaf_counts.size8, leaf_counts.size16, leaf_counts.size32, leaf_counts.size64);
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
        if (string_type->stored_size_size * 8 == leaf_size) {
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
        if (array_type->stored_size_size * 8 == leaf_size) {
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

#define SIMPLE_ERROR(message) std::cout << "spc.exe: error: " << message << std::endl

#ifdef __MINGW32__
    static inline auto realpath (const char* path, char* resolved_path) {
        return _fullpath(resolved_path, path, PATH_MAX);
    };
#endif

static inline int open_regular (const char* path, int flags, struct stat *stat_buffer) {
    int fd = open(path, flags);
    if (fd < 0) {
        SIMPLE_ERROR("could not open file");
        return 1;
    }
    if (fstat(fd, stat_buffer) != 0) {
        SIMPLE_ERROR("could not get file status");
        return 1;
    }
    if (!S_ISREG(stat_buffer->st_mode)) {
        SIMPLE_ERROR("file is not a regular file");
        return 1;
    }
    return fd;
}

int main(int argc, const char** argv) {
    std::cout << "spc.exe" << std::endl;
    if (argc <= 2) {
        SIMPLE_ERROR("no output and/or input supplied");
        return 1;
    }

    const char* input_path = argv[1];
    struct stat input_file_stat;
    int input_fd = open_regular(input_path, O_RDONLY | O_BINARY, &input_file_stat);

    const char* output_path = argv[2];
    struct stat output_file_stat;
    int output_fd = open_regular(output_path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, &output_file_stat);
    
    char input_path_buffer[PATH_MAX];
    input_file_path = realpath(input_path, input_path_buffer);
    if (!input_file_path) {
        SIMPLE_ERROR("could not get input file path");
        return 1;
    }
    
    auto input_file_size = input_file_stat.st_size;
    char input_data[input_file_size + 1];
    input_data[input_file_size] = 0;
    int read_result = read(input_fd, input_data, input_file_size);
    if (read_result != input_file_size) {
        SIMPLE_ERROR("could not read file");
        return 1;
    }
    if (close(input_fd) != 0) {
        SIMPLE_ERROR("could not close input file");
        return 1;
    }

    input_start = input_data;
    std::cout << "Lexing input of length: " << std::setprecision(0) << input_file_size << std::endl;

    auto start_ts = std::chrono::high_resolution_clock::now();

    #define DO_LEX
    #ifdef DO_LEX
    for (size_t i = 0; i < 1; i++)
    {
        lexer::IdentifierMap identifier_map;
        uint8_t __buffer[5000];
        auto buffer = Buffer(__buffer);
        auto target_struct = lexer::lex<false>(input_data, identifier_map, buffer);

        //#define DO_EXTRACT
        #ifdef DO_EXTRACT
        auto target_name = extract_string(target_struct->name);
        for_<size_t, 64, 32, 16, 8>([&target_struct, &buffer, &target_name]<size_t v>() {
            printf("size: %d\n", v);
            __extract_leafes_def<v>(lexer::KEYWORDS::STRUCT, target_struct, buffer, target_name);
        });
        #endif
        // #define DO_PRINT
        #ifdef DO_PRINT
        printf("\n\n- target: ");
        printf(extract_string(target_struct->name).c_str());
        print_parse_result(identifier_map, buffer);
        #endif

        #define DO_CODEGEN
        #ifdef DO_CODEGEN
        decode_code::generate(target_struct, buffer, output_fd);
        #endif

        //uint8_t __buffer2[target_data->internal_size + 8];
        //auto buffer2 = Buffer(__buffer2, target_data->internal_size);

        // parser::StructFieldWithOffset size8_leafes[target_data->leaf_counts.size8];
        // parser::StructFieldWithOffset size16_leafes[target_data->leaf_counts.size16];
        // parser::StructFieldWithOffset size32_leafes[target_data->leaf_counts.size32];
        // parser::StructFieldWithOffset size64_leafes[target_data->leaf_counts.size64];

        // printf("\ntarget internal size: %d\n", target_data->internal_size);
        // printf("buffer position: %d\n", buffer.current_position());
    }
    #endif

    auto end_ts = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts);
    std::cout << "\n\nTime taken: " << duration.count() << " milliseconds" << std::endl;
}
