#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include "base.cpp"
#include "constexpr_helpers.cpp"
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
    FIXED_VARIANT,
    PACKED_VARIANT,
    DYNAMIC_VARIANT,
    IDENTIFIER
};

enum class VALUE_FIELD_TYPE : uint8_t {
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
};

enum SIZE : uint8_t {
    SIZE_1,
    SIZE_2,
    SIZE_4,
    SIZE_8,
    SIZE_0 = 0xff
};

INLINE constexpr uint8_t byte_size_of (SIZE size) {
    return 1U << static_cast<uint8_t>(size);
}

template <lexer::VALUE_FIELD_TYPE field_type>
consteval lexer::SIZE get_type_alignment () {
    if constexpr (
        field_type == lexer::VALUE_FIELD_TYPE::UINT8
        || field_type == lexer::VALUE_FIELD_TYPE::INT8
    ) {
        return lexer::SIZE::SIZE_1;
    } else if constexpr (
        field_type == lexer::VALUE_FIELD_TYPE::UINT16
        || field_type == lexer::VALUE_FIELD_TYPE::INT16
    ) {
        return lexer::SIZE::SIZE_2;
    } else if constexpr (
        field_type == lexer::VALUE_FIELD_TYPE::UINT32
        || field_type == lexer::VALUE_FIELD_TYPE::INT32
        || field_type == lexer::VALUE_FIELD_TYPE::FLOAT32
    ) {
        return lexer::SIZE::SIZE_4;
    } else {
        return lexer::SIZE::SIZE_8;
    }
}
template <lexer::VALUE_FIELD_TYPE field_type>
consteval uint64_t get_type_size () {
    return byte_size_of(get_type_alignment<field_type>());
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

INLINE constexpr SIZE get_size_size (uint8_t size) {
    return SIZE::SIZE_1;
}
INLINE constexpr SIZE get_size_size (uint16_t size) {
    if (size <= UINT8_MAX) {
        return SIZE::SIZE_1;
    } else {
        return  SIZE::SIZE_2;
    }
}
INLINE constexpr SIZE get_size_size (uint32_t size) {
    if (size <= UINT8_MAX) {
        return SIZE::SIZE_1;
    } else if (size <= UINT16_MAX) {
        return  SIZE::SIZE_2;
    } else {
        return  SIZE::SIZE_4;
    }
}
INLINE constexpr SIZE get_size_size (uint64_t size) {
    if (size <= UINT8_MAX) {
        return SIZE::SIZE_1;
    } else if (size <= UINT16_MAX) {
        return  SIZE::SIZE_2;
    } else if (size <= UINT32_MAX) {
        return  SIZE::SIZE_4;
    } else {
        return  SIZE::SIZE_8;
    }
}



struct Range {
    uint32_t min;
    uint32_t max;
};

typedef uint16_t variant_count_t;

union LeafCounts {
    
    struct Counts {
        //Counts () = default;
        uint16_t size8;
        uint16_t size16;
        uint16_t size32;
        uint16_t size64;
        INLINE constexpr uint32_t bytes () const {
            return size8 + size16 * 2 + size32 * 4 + size64 * 8;
        }
        INLINE constexpr uint16_t total () const {
            return size8 + size16 + size32 + size64;
        }
    } counts;
    uint64_t as_uint64;

    LeafCounts () = default;
    INLINE constexpr LeafCounts (Counts counts) : counts(counts) {}
    INLINE constexpr LeafCounts (uint16_t size8, uint16_t size16, uint16_t size32, uint16_t size64) : counts{size8, size16, size32, size64} {}
    INLINE constexpr LeafCounts (uint64_t as_uint64) : as_uint64(as_uint64) {}
    INLINE constexpr LeafCounts (SIZE size) : as_uint64(1ULL << (static_cast<uint8_t>(size) * sizeof(uint16_t) * 8)) {}

    INLINE constexpr void operator += (const LeafCounts& other) { as_uint64 += other.as_uint64; }
    INLINE constexpr LeafCounts operator + (const LeafCounts& other) const { return {as_uint64 + other.as_uint64}; }
    INLINE constexpr uint32_t bytes () const { return counts.bytes(); }
    INLINE constexpr uint16_t total () const { return counts.total(); }

    static INLINE constexpr LeafCounts zero () { return {0, 0, 0, 0}; }
};

struct IdentifiedDefinition {
    KEYWORDS keyword;

    struct Data {
        StringSection<uint16_t> name;

        INLINE constexpr auto as_struct () const;
        INLINE constexpr auto as_enum () const;
        INLINE constexpr auto as_union () const;
        // INLINE constexpr auto as_typedef ();
    };

    const Data* data () const;

};


struct Type {
    FIELD_TYPE type;

    INLINE const auto as_fixed_string () const;
    INLINE const auto as_string () const;
    INLINE const auto as_identifier () const;
    INLINE const auto as_array () const;
    INLINE const auto as_fixed_variant () const;
    INLINE const auto as_packed_variant () const;
    INLINE const auto as_dynamic_variant () const;
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
        extended->length_size = get_size_size(length);
    }
    
    uint32_t length;
    SIZE length_size;
};
INLINE const auto Type::as_fixed_string () const {
    return get_extended_type<const FixedStringType>(this);
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
INLINE const auto Type::as_string () const {
    return get_extended_type<const StringType>(this);
}


typedef Buffer::Index<const IdentifiedDefinition> IdentifedDefinitionIndex;
struct IdentifiedType {
    INLINE static void create (Buffer &buffer, IdentifedDefinitionIndex identifier_idx) {
        auto [extended, base] = create_extended<IdentifiedType, Type>(buffer);
        base->type = IDENTIFIER;
        extended->identifier_idx = identifier_idx;
    }

    IdentifedDefinitionIndex identifier_idx;
};
INLINE const auto Type::as_identifier () const {
    return get_extended_type<const IdentifiedType>(this);
}

struct ArrayType {

    INLINE static auto create (Buffer &buffer) {
        return __create_extended<ArrayType, Type>(buffer);
    }

    uint32_t length;
    SIZE stored_size_size;
    SIZE size_size;

    INLINE const Type* inner_type () const {
        return reinterpret_cast<const Type*>(this + 1);
    }
    
};
INLINE const auto Type::as_array () const {
    return get_extended_type<const ArrayType>(this);
}



template <typename TypeMeta_T>
struct _VariantType {
    INLINE static auto create (Buffer &buffer) {
        return __create_extended<_VariantType, Type, TypeMeta_T>(buffer);
    }
    
    uint64_t min_byte_size; 
    uint16_t variant_count;
    /* Count of how many non-nested fixed size leafs are in this variant type. */
    uint16_t level_fixed_leafs;
    /* Count of how many non-nested variable size leafs are in this variant type. */
    uint16_t level_var_leafs;
    SIZE max_alignment;
    SIZE stored_size_size;
    SIZE size_size;

    INLINE const TypeMeta_T* type_metas () const {
        return get_padded<const TypeMeta_T>(this + 1);
    }

    INLINE const Type* first_variant() const {
        return reinterpret_cast<const Type*>(reinterpret_cast<size_t>(type_metas()) + variant_count * sizeof(TypeMeta_T));
    }
};


struct FixedVariantTypeMeta {
    LeafCounts fixed_leaf_counts;
    LeafCounts variant_field_counts;
};

struct DynamicVariantTypeMeta {
    LeafCounts fixed_leaf_counts;
    LeafCounts var_leaf_counts;
    LeafCounts variant_field_counts;
    uint16_t level_size_leafs;
};

typedef _VariantType<FixedVariantTypeMeta> FixedVariantType;
INLINE const auto Type::as_fixed_variant () const {
    return get_extended_type<const FixedVariantType>(this);
}

typedef _VariantType<FixedVariantTypeMeta> PackedVariantType;
INLINE const auto Type::as_packed_variant () const {
    return get_extended_type<const PackedVariantType>(this);
}

typedef _VariantType<DynamicVariantTypeMeta> DynamicVariantType;
INLINE const auto Type::as_dynamic_variant () const {
    return get_extended_type<const DynamicVariantType>(this);
}


struct StructField {
    
    struct Data {
        StringSection<uint16_t> name;

        INLINE const Type* type () const {
            return reinterpret_cast<const Type*>(this + 1);
        }
    };

    INLINE const Data* data () const {
        return get_padded<const Data>(reinterpret_cast<size_t>(this));
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
    uint16_t level_size_leafs;
    uint16_t total_variant_count;
    uint16_t level_variant_fields;
    uint16_t total_variant_fixed_leafs;
    uint16_t total_variant_var_leafs;
    uint16_t field_count;
    SIZE max_alignment;

    INLINE static auto create(Buffer &buffer) {
        return __create_extended<DefinitionWithFields, IdentifiedDefinition>(buffer);
    }

    INLINE static StructField::Data* reserve_field(Buffer &buffer) {
        return create_padded<StructField::Data>(buffer);
    }

    INLINE const StructField* first_field() const {
        return reinterpret_cast<const StructField*>(this + 1);
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

INLINE const IdentifiedDefinition::Data* IdentifiedDefinition::data () const {
    return get_extended_identifed_definition<const Data>(this);
}

INLINE constexpr auto IdentifiedDefinition::Data::as_struct () const {
    return static_cast<const StructDefinition*>(this);
}
INLINE constexpr auto IdentifiedDefinition::Data::as_enum () const {
    return static_cast<const EnumDefinition*>(this);
}
INLINE constexpr auto IdentifiedDefinition::Data::as_union () const {
    return static_cast<const UnionDefinition*>(this);
}

typedef std::unordered_map<std::string, IdentifedDefinitionIndex> IdentifierMap;

template <class _Fn, class _Ret, class... _Args>
concept invocable_r = requires (_Fn&& __fn, _Args&&... __args) {
    { std::invoke(std::forward<_Fn>(__fn), std::forward<_Args>(__args)...) } -> std::same_as<_Ret>;
};

template <
typename T,
typename U = const std::remove_const_t<T>,
typename on_simple_F,
std::invocable_r<void, const FixedStringType*> on_fixed_string_F,
std::invocable_r<void, const StringType*> on_string_F,
std::invocable_r<U*, const ArrayType*> on_fixed_array_F,
std::invocable_r<U*, const ArrayType*> on_array_F,
std::invocable_r<U*, const FixedVariantType*> on_fixed_variant_F,
std::invocable_r<U*, const PackedVariantType*> on_packed_variant_F,
std::invocable_r<U*, const DynamicVariantType*> on_dynamic_variant_F,
std::invocable_r<void, const IdentifiedType*> on_identifier_F
>
requires (
    (
        CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::BOOL>
        || CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::UINT8>
        || CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::UINT16>
        || CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::UINT32>
        || CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::UINT64>
        || CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::INT8>
        || CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::INT16>
        || CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::INT32>
        || CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::INT64>
        || CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::FLOAT32>
        || CallableWithValueTemplate<on_simple_F, VALUE_FIELD_TYPE::FLOAT64>
    )
)
INLINE U* visit_type (
    const Type* const& type,
    on_simple_F&& on_simple,
    on_fixed_string_F&& on_fixed_string,
    on_string_F&& on_string,
    on_fixed_array_F&& on_fixed_array,
    on_array_F&& on_array,
    on_fixed_variant_F&& on_fixed_variant,
    on_packed_variant_F&& on_packed_variant,
    on_dynamic_variant_F&& on_dynamic_variant,
    on_identifier_F&& on_identifier
) {
    switch (type->type) {
        case FIELD_TYPE::BOOL: {
            on_simple.template operator()<VALUE_FIELD_TYPE::BOOL>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::UINT8: {
            on_simple.template operator()<VALUE_FIELD_TYPE::UINT8>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::UINT16: {
            on_simple.template operator()<VALUE_FIELD_TYPE::UINT16>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::UINT32: {
            on_simple.template operator()<VALUE_FIELD_TYPE::UINT32>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::UINT64: {
            on_simple.template operator()<VALUE_FIELD_TYPE::UINT64>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::INT8: {
            on_simple.template operator()<VALUE_FIELD_TYPE::INT8>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::INT16: {
            on_simple.template operator()<VALUE_FIELD_TYPE::INT16>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::INT32: {
            on_simple.template operator()<VALUE_FIELD_TYPE::INT32>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::INT64: {
            on_simple.template operator()<VALUE_FIELD_TYPE::INT64>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::FLOAT32: {
            on_simple.template operator()<VALUE_FIELD_TYPE::FLOAT32>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::FLOAT64: {
            on_simple.template operator()<VALUE_FIELD_TYPE::FLOAT64>();
            return reinterpret_cast<U*>(type + 1);
        }
        case FIELD_TYPE::STRING_FIXED: {
            const FixedStringType* const fixed_string_type = type->as_fixed_string();
            on_fixed_string(fixed_string_type);
            return reinterpret_cast<U*>(fixed_string_type + 1);
        }
        case FIELD_TYPE::STRING: {
            const StringType* const string_type = type->as_string();
            on_string(string_type);
            return reinterpret_cast<U*>(string_type + 1);
        }
        case FIELD_TYPE::ARRAY_FIXED: {
            return on_fixed_array(type->as_array());
        }
        case FIELD_TYPE::ARRAY: {
            return on_array(type->as_array());
        }
        case FIELD_TYPE::FIXED_VARIANT: {
            return on_fixed_variant(type->as_fixed_variant());
        }
        case FIELD_TYPE::PACKED_VARIANT: {
            return on_packed_variant(type->as_packed_variant());
        }
        case FIELD_TYPE::DYNAMIC_VARIANT: {
            return on_dynamic_variant(type->as_dynamic_variant());
        }
        case FIELD_TYPE::IDENTIFIER: {
            const IdentifiedType* const identifier_type = type->as_identifier();
            on_identifier(identifier_type);
            return reinterpret_cast<U*>(identifier_type + 1);
        }
    }
}


template <typename T, typename U = const std::remove_const_t<T>>
U* skip_type (const Type* type) {
    switch (type->type)
    {
    case STRING_FIXED:
        return reinterpret_cast<U*>(type->as_fixed_string() + 1);
    case STRING: {
        return reinterpret_cast<U*>(type->as_string() + 1);
    case ARRAY_FIXED:
    case ARRAY: {
        return skip_type<T>(type->as_array()->inner_type());
    }
    case FIXED_VARIANT: {
        return skip_variant_type<T>(type->as_fixed_variant());
    }
    case PACKED_VARIANT: {
        return skip_variant_type<T>(type->as_packed_variant());
    }
    case DYNAMIC_VARIANT: {
        return skip_variant_type<T>(type->as_dynamic_variant());
    }
    case IDENTIFIER: {
        return reinterpret_cast<U*>(type->as_identifier() + 1);
    }
    default: {
        return reinterpret_cast<U*>(type + 1);
    }
    }
    }
}

template <typename TypeT, typename ValueT>
struct ITypeVisitorResult;

template <typename TypeT>
struct ITypeVisitorResult<TypeT, void> {
    using ConstTypeT = const std::remove_const_t<TypeT>;
    constexpr ITypeVisitorResult (ConstTypeT*&& next_type) : next_type(std::forward<ConstTypeT*>(next_type)) {}
    ConstTypeT* next_type;
};

template <typename TypeT, typename ValueT>
struct ITypeVisitorResult {
    using ConstTypeT = const std::remove_const_t<TypeT>;
    constexpr ITypeVisitorResult (ConstTypeT*&& next_type, ValueT&& value) : next_type(std::forward<ConstTypeT*>(next_type)), value(std::forward<ValueT>(value)) {}
    ConstTypeT* next_type;
    ValueT value;
};

template <typename TypeT, typename ValueT = void>
struct ITypeVisitor {
    private:
    const Type* const& type;

    public:
    static constexpr bool no_value = std::is_same_v<ValueT, void>;
    using ConstTypeT = const std::remove_const_t<TypeT>;
    using ResultT = ITypeVisitorResult<TypeT, ValueT>;

    INLINE constexpr ITypeVisitor (const Type* const& type) : type(type) {}
    
    INLINE ResultT visit () const {
        switch (type->type) {
            case FIELD_TYPE::BOOL: {
                if constexpr (no_value) {
                    on_bool();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_bool()};
                }
            }
            case FIELD_TYPE::UINT8: {
                if constexpr (no_value) {
                    on_uint8();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_uint8()};
                }
            }
            case FIELD_TYPE::UINT16: {
                if constexpr (no_value) {
                    on_uint16();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_uint16()};
                }
            }
            case FIELD_TYPE::UINT32: {
                if constexpr (no_value) {
                    on_uint32();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_uint32()};
                }
            }
            case FIELD_TYPE::UINT64: {
                if constexpr (no_value) {
                    on_uint64();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_uint64()};
                }
            }
            case FIELD_TYPE::INT8: {
                if constexpr (no_value) {
                    on_int8();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_int8()};
                }
            }
            case FIELD_TYPE::INT16: {
                if constexpr (no_value) {
                    on_int16();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_int16()};
                }
            }
            case FIELD_TYPE::INT32: {
                if constexpr (no_value) {
                    on_int32();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_int32()};
                }
            }
            case FIELD_TYPE::INT64: {
                if constexpr (no_value) {
                    on_int64();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_int64()};
                }
            }
            case FIELD_TYPE::FLOAT32: {
                if constexpr (no_value) {
                    on_float32();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_float32()};
                }
            }
            case FIELD_TYPE::FLOAT64: {
                if constexpr (no_value) {
                    on_float64();
                    return {reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(type + 1), on_float64()};
                }
            }
            case FIELD_TYPE::STRING_FIXED: {
                const FixedStringType* const fixed_string_type = type->as_fixed_string();
                if constexpr (no_value) {
                    on_fixed_string(fixed_string_type);
                    return {reinterpret_cast<ConstTypeT*>(fixed_string_type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(fixed_string_type + 1), on_fixed_string(fixed_string_type)};
                }
            }
            case FIELD_TYPE::STRING: {
                const StringType* const string_type = type->as_string();
                if constexpr (no_value) {
                    on_string(string_type);
                    return {reinterpret_cast<ConstTypeT*>(string_type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(string_type + 1), on_string(string_type)};
                }
            }
            case FIELD_TYPE::ARRAY_FIXED: {
                return on_fixed_array(type->as_array());
            }
            case FIELD_TYPE::ARRAY: {
                return on_array(type->as_array());
            }
            case FIELD_TYPE::FIXED_VARIANT: {
                return on_fixed_variant(type->as_fixed_variant());
            }
            case FIELD_TYPE::PACKED_VARIANT: {
                return on_packed_variant(type->as_packed_variant());
            }
            case FIELD_TYPE::DYNAMIC_VARIANT: {
                return on_dynamic_variant(type->as_dynamic_variant());
            }
            case FIELD_TYPE::IDENTIFIER: {
                const IdentifiedType* const identifier_type = type->as_identifier();
                if constexpr (no_value) {
                    on_identifier(identifier_type);
                    return {reinterpret_cast<ConstTypeT*>(identifier_type + 1)};
                } else {
                    return {reinterpret_cast<ConstTypeT*>(identifier_type + 1), on_identifier(identifier_type)};
                }
            }
        }
    }

    INLINE virtual ValueT on_bool () const = 0;
    INLINE virtual ValueT on_uint8 () const = 0;
    INLINE virtual ValueT on_uint16 () const = 0;
    INLINE virtual ValueT on_uint32 () const = 0;
    INLINE virtual ValueT on_uint64 () const = 0;
    INLINE virtual ValueT on_int8 () const = 0;
    INLINE virtual ValueT on_int16 () const = 0;
    INLINE virtual ValueT on_int32 () const = 0;
    INLINE virtual ValueT on_int64 () const = 0;
    INLINE virtual ValueT on_float32 () const = 0;
    INLINE virtual ValueT on_float64 () const = 0;
    
    INLINE virtual ValueT on_fixed_string (const lexer::FixedStringType*) const = 0;
    INLINE virtual ValueT on_string (const lexer::StringType*) const = 0;
    INLINE virtual ResultT on_fixed_array (const lexer::ArrayType*) const = 0;
    INLINE virtual ResultT on_array (const lexer::ArrayType*) const = 0;
    INLINE virtual ResultT on_fixed_variant (const lexer::FixedVariantType*) const = 0;
    INLINE virtual ResultT on_packed_variant (const lexer::PackedVariantType*) const = 0;
    INLINE virtual ResultT on_dynamic_variant (const lexer::DynamicVariantType*) const = 0;
    INLINE virtual ValueT on_identifier (const lexer::IdentifiedType*) const = 0;
};

template <typename T, typename TypeMeta_T, typename U = const std::remove_const_t<T>>
INLINE U* skip_variant_type (const _VariantType<TypeMeta_T>* variant_type) {
    auto types_count = variant_type->variant_count;
    const Type* type = variant_type->first_variant();
    for (uint16_t i = 0; i < types_count; i++) {
        type = skip_type<Type>(type);
    }
    return reinterpret_cast<const U*>(type);
}

}