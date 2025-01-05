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


struct SizeLeaf {
    uint32_t leaf_idx;
    uint32_t min_size;
    lexer::SIZE size_size;
    lexer::SIZE stored_size_size;
};

struct CodeChunks {
    uint32_t current_map_idx;
    lexer::LeafCounts::Counts fixed_leaf_counts;
    lexer::LeafCounts::Counts fixed_leaf_positions;
    lexer::LeafCounts::Counts var_leaf_counts;
    lexer::LeafCounts::Counts var_leaf_positions;
    uint32_t total_fixed_leafs;
    uint32_t total_var_leafs;
    uint32_t total_leafs;
    uint16_t current_size_leaf_idx;

    INLINE auto sizes () {
        return reinterpret_cast<uint64_t*>(reinterpret_cast<size_t>(this) + sizeof(CodeChunks));
    }
    INLINE auto chunk_starts () {
        return reinterpret_cast<Buffer::Index<char>*>(reinterpret_cast<size_t>(this) + sizeof(CodeChunks) + total_leafs * sizeof(uint64_t));
    }
    INLINE auto chunk_map () {
        return reinterpret_cast<uint32_t*>(reinterpret_cast<size_t>(this) + sizeof(CodeChunks) + total_leafs * (sizeof(uint64_t)+ sizeof(Buffer::Index<char>)));
    }
    INLINE auto size_leafe_idxs () {
        return reinterpret_cast<uint16_t*>(reinterpret_cast<size_t>(this) + sizeof(CodeChunks) + total_leafs * (sizeof(uint64_t) + sizeof(Buffer::Index<char>) + sizeof(uint32_t)));
    }
    INLINE auto size_leafs () {
        return reinterpret_cast<SizeLeaf*>(reinterpret_cast<size_t>(this) + sizeof(CodeChunks) + total_leafs * (sizeof(uint64_t) + sizeof(Buffer::Index<char>) + sizeof(uint32_t)) + total_var_leafs * sizeof(uint16_t));
    }

    template <lexer::SIZE size, bool is_varsize>
    INLINE auto next_fixed (uint32_t idx, uint64_t length) {
        if constexpr (is_varsize) {
            *(size_leafe_idxs() + idx) = current_size_leaf_idx;
            idx += total_fixed_leafs;
        }
        *(sizes() + idx) = size * length;
        uint32_t map_idx = current_map_idx++;
        printf("map_idx: %d, idx: %d, size: %d, %s\n", map_idx, idx, size, is_varsize ? "var" : "fixed");
        *(chunk_map() + map_idx) = idx;
        
        return chunk_starts() + idx;
    }
    template <bool is_varsize>
    INLINE std::pair<lexer::LeafCounts::Counts&, lexer::LeafCounts::Counts&> get_counts_and_positions () {
        if constexpr (is_varsize) {
            return {var_leaf_counts, var_leaf_positions};
        } else {
            return {fixed_leaf_counts, fixed_leaf_positions};
        }
    }
    template <bool is_varsize>
    INLINE uint32_t reserve_idx (lexer::SIZE size) {
        auto [counts, positions] = get_counts_and_positions<is_varsize>();
        switch (size) {
        case lexer::SIZE_1:
            if (positions.size8 == counts.size8) {
                printf("position overflowA\n");
            }
            return counts.size64 + counts.size32 + counts.size16 + positions.size8++;
        case lexer::SIZE_2:
            if (positions.size16 == counts.size16) {
                printf("position overflowA\n");
            }
            return counts.size64 + counts.size32 + positions.size16++;
        case lexer::SIZE_4:
            if (positions.size32 == counts.size32) {
                printf("position overflowA\n");
            }
            return counts.size64 + positions.size32++;
        case lexer::SIZE_8:
            if (positions.size64 == counts.size64) {
                printf("position overflowA\n");
            }
            return positions.size64++;
        default:
            printf("invalid size\n");
            exit(1);
        }
    }
    template <bool is_varsize>
    INLINE auto reserve_size8 (uint64_t length) {
        auto [counts, positions] = get_counts_and_positions<is_varsize>();
        if (positions.size8 == counts.size8) {
            printf("position overflow\n");
            printf(is_varsize ? "var" : "fixed");
        }
        uint32_t idx = counts.size64 + counts.size32 + counts.size16 + positions.size8++;
        return next_fixed<lexer::SIZE_1, is_varsize>(idx, length);

    }
    template <bool is_varsize>
    INLINE auto reserve_size16 (uint64_t length) {
        auto [counts, positions] = get_counts_and_positions<is_varsize>();
        if (positions.size16 == counts.size16) {
            printf("position overflow\n");
            printf(is_varsize ? "var" : "fixed");
        }
        uint32_t idx = counts.size64 + counts.size32 + positions.size16++;
        return next_fixed<lexer::SIZE_2, is_varsize>(idx, length);
    }
    template <bool is_varsize>
    INLINE auto reserve_size32 (uint64_t length) {
        auto [counts, positions] = get_counts_and_positions<is_varsize>();
        if (positions.size32 == counts.size32) {
            printf("position overflow\n");
            printf(is_varsize ? "var" : "fixed");
        }
        uint32_t idx = counts.size64 + positions.size32++;
        return next_fixed<lexer::SIZE_4, is_varsize>(idx, length);
    }
    template <bool is_varsize>
    INLINE auto reserve_size64 (uint64_t length) {
        auto [counts, positions] = get_counts_and_positions<is_varsize>();
        if (positions.size64 == counts.size64) {
            printf("position overflow\n");
            printf(is_varsize ? "var" : "fixed");
        }
        uint32_t idx = positions.size64++;
        return next_fixed<lexer::SIZE_8, is_varsize>(idx, length);
    }
    template <bool is_varsize>
    INLINE auto reserve_next (lexer::SIZE size, uint64_t length) {
        switch (size) {
        case lexer::SIZE_1:
            return reserve_size8<is_varsize>(length);
        case lexer::SIZE_2:
            return reserve_size16<is_varsize>(length);
        case lexer::SIZE_4:
            return reserve_size32<is_varsize>(length);
        case lexer::SIZE_8:
            return reserve_size64<is_varsize>(length);
        default:
            printf("invalid size\n");
            exit(1);
        }
    }
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

INLINE ArrayCtorStrs make_array_ctor_strs (uint8_t array_depth) {
    std::string ctro_args = "size_t __base__";
    std::string ctor_inits = "__base__(__base__)";
    std::string ctor_used = "return {__base__";


    for (uint32_t i = 0; i < array_depth; i++) {
        std::string idx_str = "idx_" + std::to_string(i);
        ctro_args += ", uint32_t " + idx_str;
        ctor_inits += ", " + idx_str + "(" + idx_str + ")";
        ctor_used += ", " + idx_str;
    }

    ctor_used += "};";

    return {ctro_args, ctor_inits, ctor_used};
}

INLINE uint64_t get_idx_size_multiplier (uint32_t* array_lengths, uint8_t array_depth, uint8_t i) {
    uint64_t size = 1;
    uint32_t* array_lengths_end = array_lengths + array_depth - 1 - i;
    for (uint32_t* array_length = array_lengths; array_length < array_lengths_end; array_length++) {
        size *= *array_length;
    }
    return size;
}

template <bool no_multiply>
INLINE std::string make_idx_calc (uint32_t* array_lengths, uint8_t array_depth) {
    

    if (array_depth == 1) {
        return "idx_0";
    } else {
        auto add_idx = [array_lengths, array_depth]<bool is_last>(uint32_t i)->std::string {
            if constexpr (is_last) {
                return " + idx_" + std::to_string(i);
            } else {
                return " + idx_" + std::to_string(i) + " * " + std::to_string(get_idx_size_multiplier(array_lengths, array_depth, i));
            }
        };

        std::string idx_calc;

        if  constexpr (no_multiply) {
            idx_calc = "idx_0 * " + std::to_string(get_idx_size_multiplier(array_lengths, array_depth, 0));
        } else {
            idx_calc = "(idx_0 * " + std::to_string(get_idx_size_multiplier(array_lengths, array_depth, 0));
        }

        for (uint32_t i = 1; i < array_depth - 1; i++) {
            idx_calc += add_idx.template operator()<false>(i);
        }

        if constexpr (no_multiply) {
            return idx_calc + add_idx.template operator()<true>(array_depth - 1);
        } else {
            return idx_calc + add_idx.template operator()<true>(array_depth - 1) + ")";
        }
    }
}

template <typename T, typename Derived>
struct GenStructFieldResult {
    T* next;
    Derived code;
};

template <bool is_varsize>
GenStructFieldResult<lexer::StructField, codegen::UnknownNestedStruct> gen_fixed_array_element (lexer::Type* inner_type, Buffer &buffer, codegen::UnknownNestedStruct array_struct, std::string base_name, CodeChunks* leafs, uint64_t outer_array_length, uint32_t* array_lengths, uint8_t array_depth, uint8_t element_depth);

template <bool is_varsize>
GenStructFieldResult<lexer::StructField, codegen::__UnknownStruct> gen_struct_field (lexer::StructField::Data* field, Buffer &buffer, codegen::__UnknownStruct code, std::string base_name, CodeChunks* leafs, uint64_t outer_array_length, uint32_t* array_lengths, uint8_t depth, uint8_t array_depth) {
    using T = lexer::StructField;
    auto field_type = field->type();
    auto name = extract_string(field->name);
    auto unique_name = name + "_" + std::to_string(depth);
    switch (field_type->type)
    {
    case lexer::FIELD_TYPE::STRING_FIXED: {
        auto fixed_string_type = field_type->as_fixed_string();
        uint32_t length = fixed_string_type->length;
        auto size_type_str =  get_size_type_str(length);
        auto chunk_start_ptr = leafs->reserve_size8<is_varsize>(outer_array_length * length);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto string_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);
        
        if (array_depth == 0) {
            string_struct = string_struct
            .method("const char* c_str")
                ("return add_offset<const char*>(__base__, ")(");", chunk_start_ptr).nl()
            .end();
        } else {
            string_struct = string_struct
            .method("const char* c_str")
                ("return add_offset<const char*>(__base__, ")(" + ", chunk_start_ptr)(make_idx_calc<true>(array_lengths, array_depth))(length)(");").nl()
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
    case lexer::FIELD_TYPE::STRING: {
        auto string_type = field_type->as_string();
        lexer::SIZE size_size = string_type->size_size;
        lexer::SIZE stored_size_size = string_type->stored_size_size;
        auto size_type_str = get_size_type_str(size_size);
        auto chunk_start_ptr = leafs->reserve_size8<true>(1);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        uint32_t leaf_idx = leafs->reserve_idx<false>(stored_size_size);
        leafs->sizes()[leaf_idx] = stored_size_size;

        uint16_t size_leaf_idx = leafs->current_size_leaf_idx++;
        leafs->size_leafs()[size_leaf_idx] = {leaf_idx, string_type->min_length, size_size, stored_size_size};

        code = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits)
            .method("const char* c_str")
                ("return add_offset<const char*>(__base__, ")(");", chunk_start_ptr).nl()
            .end()
            .method(size_type_str + " size")
                ("return ")(base_name)("::size")(size_leaf_idx)("(__base__);").nl()
            .end()
            .method(size_type_str + " length")
                .line("return size() - 1;")
            .end()
            ._private()
            .field("size_t", "__base__")
        .end()
        .method(unique_name + " " + name)
            .line(array_ctor_strs.ctor_used)
        .end();

        return {(T*)(string_type + 1), code};
    }
    case lexer::FIELD_TYPE::ARRAY_FIXED: {
        auto array_type = field_type->as_array();
        uint32_t length = array_type->length;
        auto size_type_str = get_size_type_str(length);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        codegen::NestedStruct<codegen::__UnknownStruct> array_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);

        uint32_t new_array_lengths[array_depth];
        if (array_depth > 0) {
            uint8_t last = array_depth - 1;
            for (uint32_t i = 0; i < last; i++) {
                new_array_lengths[i] = array_lengths[i];
            }
            new_array_lengths[last] = length;
        }

        auto result = gen_fixed_array_element<is_varsize>(array_type->inner_type(), buffer, codegen::UnknownNestedStruct{array_struct}, base_name, leafs, array_type->length * outer_array_length, new_array_lengths, array_depth + 1, 0);

        array_struct = codegen::NestedStruct<codegen::__UnknownStruct>{result.code}
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
        if constexpr(is_varsize) {
            INTERNAL_ERROR("Dynamic array cant be nested");
        }
        auto array_type = field_type->as_array();
        lexer::SIZE size_size = array_type->size_size;
        lexer::SIZE stored_size_size = array_type->stored_size_size;
        auto size_type_str = get_size_type_str(size_size);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(0);

        auto array_struct = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);

        auto result = gen_fixed_array_element<true>(array_type->inner_type(), buffer, codegen::UnknownNestedStruct{array_struct}, base_name, leafs, 1, nullptr, 1, 0);

        uint32_t leaf_idx = leafs->reserve_idx<false>(stored_size_size);
        leafs->sizes()[leaf_idx] = stored_size_size;

        uint16_t size_leaf_idx = leafs->current_size_leaf_idx++;
        leafs->size_leafs()[size_leaf_idx] = {leaf_idx, array_type->length, size_size, stored_size_size};

        code = codegen::NestedStruct<codegen::__UnknownStruct>{result.code}
            .method(size_type_str + " length")
                ("return ")(base_name)("::size")(size_leaf_idx)("(__base__);").nl()
            .end()
            ._private()
            .field("size_t", "__base__")
            .field("uint32_t", "idx_0")
        .end()
        .method(unique_name + " " + name)
            .line(array_ctor_strs.ctor_used)
        .end();

        return {result.next, code};
    }
    case lexer::FIELD_TYPE::IDENTIFIER: {
        auto identified_type = field_type->as_identifier();
        auto identifier = buffer.get(identified_type->identifier_idx);
        if (identifier->keyword != lexer::KEYWORDS::STRUCT) {
            INTERNAL_ERROR("not implemented");
        }

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto struct_type = identifier->data()->as_struct();
        codegen::NestedStruct<codegen::__UnknownStruct> struct_code = code
        ._struct(unique_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);

        auto field = struct_type->first_field();
        for (uint32_t i = 0; i < struct_type->field_count; i++) {
            auto field_data = field->data();
            auto result = gen_struct_field<is_varsize>(field_data, buffer, codegen::__UnknownStruct{struct_code}, base_name, leafs, outer_array_length, array_lengths, depth + 1, array_depth);
            field = result.next;
            struct_code = codegen::NestedStruct<codegen::__UnknownStruct>{result.code};
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
        static const lexer::SIZE type_sizes[] = { lexer::SIZE_1, lexer::SIZE_1, lexer::SIZE_2, lexer::SIZE_4, lexer::SIZE_8, lexer::SIZE_1, lexer::SIZE_2, lexer::SIZE_4, lexer::SIZE_8, lexer::SIZE_4, lexer::SIZE_8 };
        std::string type_name = types[field_type->type];
        lexer::SIZE type_size = type_sizes[field_type->type];
        auto chunk_start_ptr = leafs->reserve_next<is_varsize>(type_size, outer_array_length);
        if (array_depth == 0) {
            code = code
            .method(type_name + " " + name)
                ("return *add_offset<")(type_name)(">(__base__, ")(");", chunk_start_ptr).nl()
            .end();
        } else {
            if (type_size == 1) {
                code = code
                .method(type_name + " " + name)
                    ("return *add_offset<")(type_name)(">(__base__, ")(" + ", chunk_start_ptr)(make_idx_calc<true>(array_lengths, array_depth))(");").nl()
                .end();
            } else {
                code = code
                .method(type_name + " " + name)
                    ("return *add_offset<")(type_name)(">(__base__, ")(" + ", chunk_start_ptr)(make_idx_calc<false>(array_lengths, array_depth))(" *")(type_size_strs[field_type->type])(");").nl()
                .end();
            }
        }
        return {(T*)(field_type + 1), code};
    }
    }
}

template <bool is_varsize>
GenStructFieldResult<lexer::StructField, codegen::UnknownNestedStruct> gen_fixed_array_element (lexer::Type* inner_type, Buffer &buffer, codegen::UnknownNestedStruct array_struct, std::string base_name, CodeChunks* leafs, uint64_t outer_array_length, uint32_t* array_lengths, uint8_t array_depth, uint8_t element_depth) {
    using T = lexer::StructField;
    switch (inner_type->type)
    {
    case lexer::FIELD_TYPE::STRING_FIXED: {
        auto fixed_string_type = inner_type->as_fixed_string();
        uint32_t length = fixed_string_type->length;
        auto size_type_str = get_size_type_str(length);
        // std::string element_struct_name = "String_" + array_field_name;
        auto chunk_start_ptr = leafs->reserve_size8<is_varsize>(outer_array_length * length);

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto string_struct = array_struct
        ._struct("String")
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits)
            .method("const char* c_str")
                ("return reinterpret_cast<const char*>(__base__ + ")(" + (", chunk_start_ptr)(make_idx_calc<true>(array_lengths, array_depth))(") *")(length)(");").nl()
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

        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        codegen::NestedStruct<codegen::UnknownNestedStruct> sub_array_struct = array_struct
        ._struct(element_struct_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);

        uint32_t new_array_lengths[array_depth];
        if (array_depth > 0) {
            uint8_t last = array_depth - 1;
            for (uint32_t i = 0; i < last; i++) {
                new_array_lengths[i] = array_lengths[i];
            }
            new_array_lengths[last] = length;
        }

        auto result = gen_fixed_array_element<is_varsize>(array_type->inner_type(), buffer, codegen::UnknownNestedStruct{sub_array_struct}, base_name, leafs, array_type->length * outer_array_length, new_array_lengths, array_depth + 1, element_depth + 1);

        sub_array_struct = codegen::NestedStruct<codegen::UnknownNestedStruct>{result.code}
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
        
        ArrayCtorStrs array_ctor_strs = make_array_ctor_strs(array_depth);

        auto element_struct = array_struct
        ._struct(element_struct_name)
            .ctor(array_ctor_strs.ctro_args, array_ctor_strs.ctor_inits);

        auto field = struct_type->first_field();
        for (uint32_t i = 0; i < struct_type->field_count; i++) {
            auto field_data = field->data();
            auto result = gen_struct_field<is_varsize>(field_data, buffer, codegen::__UnknownStruct{element_struct}, base_name, leafs, outer_array_length, array_lengths, 0, array_depth);
            field = result.next;
            element_struct = codegen::NestedStruct<codegen::UnknownNestedStruct>{result.code};
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
        static const lexer::SIZE type_sizes[] = { lexer::SIZE_1, lexer::SIZE_1, lexer::SIZE_2, lexer::SIZE_4, lexer::SIZE_8, lexer::SIZE_1, lexer::SIZE_2, lexer::SIZE_4, lexer::SIZE_8, lexer::SIZE_4, lexer::SIZE_8 };
        std::string type_name = types[inner_type->type];
        lexer::SIZE type_size = type_sizes[inner_type->type];
        auto chunk_start_ptr = leafs->reserve_next<is_varsize>(type_size, outer_array_length);
        if (array_depth == 0) {
            array_struct = array_struct
            .method(type_name + " get", "uint32_t index")
                ("return *add_offset<")(type_name)(">(__base__, ")(" + index *", chunk_start_ptr)(size_type_strs[inner_type->type])(");").nl()
            .end();
        } else {
            if (type_size == 1) {
                array_struct = array_struct
                .method(type_name + " get", "uint32_t idx_" + std::to_string(array_depth - 1))
                    ("return *add_offset<")(type_name)(">(__base__, ")(" + ", chunk_start_ptr)(make_idx_calc<true>(array_lengths, array_depth))(");").nl()
                .end();
            } else {
                array_struct = array_struct
                .method(type_name + " get", "uint32_t idx_" + std::to_string(array_depth - 1))
                    ("return *add_offset<")(type_name)(">(__base__, ")(" + ", chunk_start_ptr)(make_idx_calc<false>(array_lengths, array_depth))(" * ")(size_type_strs[inner_type->type])(");").nl()
                .end();
            }
        }
        return {(T*)(inner_type + 1), array_struct};
    }
    }
}

void generate (lexer::StructDefinition* target_struct, Buffer& buffer) {
    
    uint8_t _buffer[5000];
    auto code = codegen::create_code(_buffer);
    auto fixed_leafs_count = target_struct->fixed_leafs_count.counts;
    auto var_leafs_count = target_struct->var_leafs_count.counts;
    auto total_fixed_leafs = fixed_leafs_count.total();
    auto total_var_leafs = var_leafs_count.total();
    auto total_leafs = total_fixed_leafs + total_var_leafs;
    auto size_leafs_count = target_struct->size_leafs_count;
    printf("fixed_field_counts: %d\n", total_fixed_leafs);
    printf("var_field_counts: %d\n", total_var_leafs);

    uint8_t _mem[sizeof(CodeChunks) + total_leafs * (sizeof(uint64_t) + sizeof(Buffer::Index<char>) + sizeof(uint32_t)) + total_var_leafs * sizeof(uint16_t) + size_leafs_count * sizeof(SizeLeaf)];
    CodeChunks* code_chunks = reinterpret_cast<CodeChunks*>(_mem);
    code_chunks->current_map_idx = 0;
    code_chunks->fixed_leaf_counts = fixed_leafs_count;
    code_chunks->fixed_leaf_positions = {0, 0, 0, 0};
    code_chunks->var_leaf_counts = var_leafs_count;
    code_chunks->var_leaf_positions = {0, 0, 0, 0};
    code_chunks->total_fixed_leafs = total_fixed_leafs;
    code_chunks->total_var_leafs = total_var_leafs;
    code_chunks->total_leafs = total_leafs;
    code_chunks->current_size_leaf_idx = 0;
    
    std::string struct_name = extract_string(target_struct->name);
    auto struct_code = code
    ._struct(struct_name)
        .ctor("size_t __base__", "__base__(__base__)");

    auto field = target_struct->first_field();
    for (uint16_t i = 0; i < target_struct->field_count; i++) {
        auto field_data = field->data();
        auto result = gen_struct_field<false>(field_data, buffer, codegen::__UnknownStruct{struct_code}, struct_name, code_chunks, 1, nullptr, 0, 0);
        field = result.next;
        struct_code = codegen::Struct<decltype(struct_code)::__Last>{result.code};
    }

    struct_code = struct_code
    ._private()
    .field("size_t", "__base__");

    for (uint16_t i = 0; i < size_leafs_count; i++) {
        auto [leaf_idx, min_size, size_size, stored_size_size] = code_chunks->size_leafs()[i];
        uint32_t map_idx = code_chunks->current_map_idx++;
        printf("size_leaf map_idx: %d, leaf_idx: %d\n", map_idx, leaf_idx);
        code_chunks->chunk_map()[map_idx] = leaf_idx;
        struct_code = struct_code
        .method("static " + get_size_type_str(size_size) + " size" + std::to_string(i), "size_t __base__")
            ("return *add_offset<")(get_size_type_str(stored_size_size))(">(__base__, ")(");", (code_chunks->chunk_starts() + leaf_idx)).nl()
        .end();

    }

    uint64_t offset = 0;
    for (uint32_t i = 0; i < total_fixed_leafs; i++) {
        uint64_t* size_ptr = code_chunks->sizes() + i;
        uint64_t size = *size_ptr;
        *size_ptr = offset;
        offset += size;
    }

    if (total_var_leafs > 0) {
        uint8_t max_var_leaf_align;
        if (var_leafs_count.size64 > 0) {
            max_var_leaf_align = 8;
        } else if (var_leafs_count.size32 > 0) {
            max_var_leaf_align = 4;
        } else if (var_leafs_count.size16 > 0) {
            max_var_leaf_align = 2;
        } else {
            goto done;
        }
        {
            // Add padding based on alignment
            size_t mod = offset % max_var_leaf_align;
            size_t padding = (max_var_leaf_align - mod) & (max_var_leaf_align - 1);
            offset += padding;
        }
        done:;
    }
    std::string last_fixed_offset_str = std::to_string(offset);

    auto code_done = struct_code
    .end()
    .end();

    for (uint32_t j = 0; j < total_leafs; j++) {
        uint32_t i = code_chunks->chunk_map()[j];
        printf("chunk_map[%d]: %d\n", j, i);
    }
    
    char* last_offset_str = code_done.buffer().get<char>({0});
    for (uint32_t j = 0; j < total_leafs; j++) {
        uint32_t i = code_chunks->chunk_map()[j];
        auto chunk_start = code_chunks->chunk_starts()[i];
        auto offset_str = code_done.buffer().get(chunk_start);
        
        char end_backup = *offset_str;
        *offset_str = 0;
        printf(last_offset_str);
        *offset_str = end_backup;
        last_offset_str = offset_str;
        
        if (i < total_fixed_leafs) {
            printf(std::to_string(code_chunks->sizes()[i]).c_str());
        } else {
            printf(last_fixed_offset_str.c_str());
            uint64_t size_leafs[size_leafs_count] = {0};
            uint16_t known_size_leafs = 0;
            for (uint32_t h = total_fixed_leafs; h < i; h++) {
                uint16_t size_leaf_idx = code_chunks->size_leafe_idxs()[h - total_fixed_leafs];
                uint64_t* size_leaf = size_leafs + size_leaf_idx;
                if (*size_leaf == 0) {
                    known_size_leafs++;
                }
                *size_leaf += code_chunks->sizes()[h];
            }
            for (uint16_t size_leaf_idx = 0; size_leaf_idx < known_size_leafs; size_leaf_idx++) {
                printf(" + ");
                printf(struct_name.c_str());
                printf("::size");
                printf(std::to_string(size_leaf_idx).c_str());
                uint64_t size = size_leafs[size_leaf_idx];
                if (size == 1) {
                    printf("(__base__)");
                } else {
                    printf("(__base__) * ");
                    printf(std::to_string(size).c_str());
                }
            }
        }
    }
    printf(last_offset_str);

    code_done.dispose();
}


}