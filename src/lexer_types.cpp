#pragma once

#include <cstdint>
#include <utility>
#include <unordered_map>
#include "base.cpp"
#include "string_helpers.cpp"
#include "memory.cpp"
#include "memory_helpers.cpp"

namespace lexer {

enum KEYWORDS : uint8_t {
    STRUCT,
    ENUM,
    UNION,
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
    PACKED_VARIANT,
    DYNAMIC_VARIANT,
    IDENTIFIER
};

enum class SIZE : uint8_t {
    SIZE_1,
    SIZE_2,
    SIZE_4,
    SIZE_8,
    SIZE_0 = 0xff
};

constexpr INLINE uint8_t byte_size_of (SIZE size) {
    return 1U << static_cast<uint8_t>(size);
}

template <Integral T>
INLINE constexpr T next_multiple (T value, SIZE base) {
    T mask = (static_cast<T>(1) << static_cast<uint8_t>(base)) - 1;
    return (value + mask) & ~mask;
}

template <Integral T>
INLINE constexpr T next_multiple (T value, T base) {
    T mask = base - 1;
    return (value + mask) & ~mask;
}


struct Range {
    uint32_t min;
    uint32_t max;
};

typedef uint16_t variant_count_t;

union LeafCounts {
    
    struct Counts {
        uint16_t size8;
        uint16_t size16;
        uint16_t size32;
        uint16_t size64;
        constexpr INLINE uint32_t bytes () const {
            return size8 + size16 * 2 + size32 * 4 + size64 * 8;
        }
        constexpr INLINE uint16_t total () const {
            return size8 + size16 + size32 + size64;
        }
    } counts;
    uint64_t as_uint64;

    LeafCounts () = default;
    constexpr INLINE LeafCounts (Counts counts) : counts(counts) {}
    constexpr INLINE LeafCounts (uint16_t size8, uint16_t size16, uint16_t size32, uint16_t size64) : counts{size8, size16, size32, size64} {}
    constexpr INLINE LeafCounts (uint64_t as_uint64) : as_uint64(as_uint64) {}
    constexpr INLINE LeafCounts (SIZE size) : as_uint64(1ULL << (static_cast<uint8_t>(size) * sizeof(uint16_t) * 8)) {}

    constexpr INLINE void operator += (const LeafCounts& other) { as_uint64 += other.as_uint64; }
    constexpr INLINE LeafCounts operator + (const LeafCounts& other) const { return {as_uint64 + other.as_uint64}; }
    constexpr INLINE uint32_t bytes () const { return counts.bytes(); }
    constexpr INLINE uint16_t total () const { return counts.total(); }
};

struct IdentifiedDefinition {
    KEYWORDS keyword;

    struct Data {
        StringSection<uint16_t> name;

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
    INLINE auto as_packed_variant () const;
    INLINE auto as_dynamic_variant () const;
};


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
    INLINE static void create (Buffer &buffer, uint32_t min_length, SIZE stored_size_size, SIZE size_size) {
        auto [extended, base] = create_extended<StringType, Type>(buffer);
        base->type = STRING;
        extended->min_length = min_length;
        extended->stored_size_size = stored_size_size;
        extended->size_size = size_size;
    }
    uint32_t min_length;
    SIZE stored_size_size;
    SIZE size_size;
};
INLINE auto Type::as_string () const {
    return get_extended_type<StringType>(this);
}


typedef Buffer::Index<IdentifiedDefinition> IdentifedDefinitionIndex;
struct IdentifiedType {
    INLINE static void create (Buffer &buffer, IdentifedDefinitionIndex identifier_idx) {
        auto [extended, base] = create_extended<IdentifiedType, Type>(buffer);
        base->type = IDENTIFIER;
        extended->identifier_idx = identifier_idx;
    }

    IdentifedDefinitionIndex identifier_idx;
};
INLINE auto Type::as_identifier () const {
    return get_extended_type<IdentifiedType>(this);
}

struct ArrayType {

    INLINE static auto create (Buffer &buffer) {
        return __create_extended<ArrayType, Type>(buffer);
    }

    uint32_t length;
    SIZE stored_size_size;
    SIZE size_size;

    INLINE Type* inner_type () {
        return reinterpret_cast<Type*>(this + 1);
    }
    
};
INLINE auto Type::as_array () const {
    return get_extended_type<ArrayType>(this);
}



template <typename TypeMeta_T>
struct _VariantType {
    INLINE static auto create (Buffer &buffer) {
        return __create_extended<_VariantType, Type, TypeMeta_T>(buffer);
    }
    
    uint64_t min_byte_size;
    uint16_t variant_count;
    uint16_t level_variant_leafs;
    SIZE max_alignment;
    SIZE stored_size_size;
    SIZE size_size;

    INLINE TypeMeta_T* type_metas () {
        return get_padded<TypeMeta_T>(this + 1);
    }

    INLINE Type* first_variant() {
        return reinterpret_cast<Type*>(reinterpret_cast<size_t>(type_metas()) + variant_count * sizeof(TypeMeta_T));
    }
};


struct VariantTypeMeta {
    LeafCounts leaf_counts;
    LeafCounts variant_field_counts;
};

struct DynamicVariantTypeMeta {
    LeafCounts leaf_counts;
    LeafCounts variant_field_counts;
    LeafCounts var_leaf_counts;
    uint16_t size_leafs_count;
};

typedef _VariantType<VariantTypeMeta> VariantType;
INLINE auto Type::as_variant () const {
    return get_extended_type<VariantType>(this);
}

typedef _VariantType<VariantTypeMeta> PackedVariantType;
INLINE auto Type::as_packed_variant () const {
    return get_extended_type<PackedVariantType>(this);
}

typedef _VariantType<DynamicVariantTypeMeta> DynamicVariantType;
INLINE auto Type::as_dynamic_variant () const {
    return get_extended_type<DynamicVariantType>(this);
}


struct StructField {
    
    struct Data {
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

struct DefinitionWithFields : IdentifiedDefinition::Data {
    LeafCounts fixed_leaf_counts;
    LeafCounts var_leafs_count;
    LeafCounts variant_field_counts;
    uint64_t min_byte_size;
    uint64_t max_byte_size;
    uint16_t size_leafs_count;
    uint16_t level_variant_fields;
    uint16_t total_variant_leafs;
    uint16_t field_count;
    SIZE max_alignment;

    INLINE static auto create(Buffer &buffer) {
        return __create_extended<DefinitionWithFields, IdentifiedDefinition>(buffer);
    }

    INLINE static StructField::Data* reserve_field(Buffer &buffer) {
        return create_padded<StructField::Data>(buffer);
    }

    INLINE StructField* first_field() {
        return reinterpret_cast<StructField*>(this + 1);
    }
};


typedef DefinitionWithFields StructDefinition;
typedef DefinitionWithFields UnionDefinition;

struct EnumDefinition : IdentifiedDefinition::Data {
    uint16_t field_count;
    SIZE type_size;

    INLINE static auto create(Buffer &buffer) {
        return __create_extended<EnumDefinition, IdentifiedDefinition>(buffer);
    }

    INLINE static EnumField* reserve_field(Buffer &buffer) {
        return buffer.get_next_aligned<EnumField>();
    }

    INLINE EnumField* first_field() {
        return reinterpret_cast<EnumField*>(this + 1);
    }
};


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

typedef std::unordered_map<std::string, IdentifedDefinitionIndex> IdentifierMap;


template <typename T>
T* skip_type (Type* type);

template <typename T, typename U>
INLINE T* skip_variant_type (_VariantType<U>* variant_type) {
    auto types_count = variant_type->variant_count;
    Type* type = variant_type->first_variant();
    for (uint16_t i = 0; i < types_count; i++) {
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
    case PACKED_VARIANT: {
        return skip_variant_type<T>(type->as_packed_variant());
    }
    case DYNAMIC_VARIANT: {
        return skip_variant_type<T>(type->as_dynamic_variant());
    }
    case IDENTIFIER: {
        return reinterpret_cast<T*>(type->as_identifier() + 1);
    }
    default: {
        return reinterpret_cast<T*>(type + 1);
    }
    }
    }
}

}