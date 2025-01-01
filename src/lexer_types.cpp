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

    INLINE uint32_t total () const { return size8 + size16 + size32 + size64; }
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

struct VariantType {
    INLINE static Buffer::Index<VariantType> create (Buffer &buffer) {
        auto created = __create_extended<VariantType, Type>(buffer);
        buffer.get(created.base)->type = VARIANT;
        return created.extended;
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
    uint16_t field_count;
    uint16_t varsize_field_count;

    INLINE static auto create(Buffer &buffer) {
        __CreateExtendedResult<DefinitionWithFields, IdentifiedDefinition> result = __create_extended<DefinitionWithFields, IdentifiedDefinition>(buffer);
        return result;
    }

    INLINE static StructField::Data* reserve_field(Buffer &buffer) {
        size_t padding = get_padding<StructField::Data>(buffer.current_position());
        auto idx = buffer.get_next_multi_byte<StructField::Data>(sizeof_v<StructField::Data> + padding);
        return buffer.get_aligned(idx.add(padding));
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
        __CreateExtendedResult<EnumDefinition, IdentifiedDefinition> result = __create_extended<EnumDefinition, IdentifiedDefinition>(buffer);
        return result;
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

}