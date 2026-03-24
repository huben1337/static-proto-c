#pragma once

#include <bit>
#include <cstdint>
#include <gsl/util>
#include <limits>
#include <span>
#include <type_traits>
#include <string_view>
#include <utility>
#include <boost/unordered/unordered_flat_map.hpp>

#include "../base.hpp"
#include "../container/memory.hpp"
#include "./memory_helpers.hpp"
#include "../estd/utility.hpp"
#include "../util/logger.hpp"
#include "../core/SIZE.hpp"
#include "../core/AlignCounts.hpp"

namespace lexer {

enum KEYWORDS : uint8_t {
    STRUCT,
    ENUM,
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

namespace {
    template <FIELD_TYPE field_type>
    constexpr SIZE type_alignment {};

    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::UINT8  > = SIZE::SIZE_1;
    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::INT8   > = SIZE::SIZE_1;
    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::BOOL   > = SIZE::SIZE_1;

    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::UINT16 > = SIZE::SIZE_2;
    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::INT16  > = SIZE::SIZE_2;

    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::UINT32 > = SIZE::SIZE_4;
    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::INT32  > = SIZE::SIZE_4;
    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::FLOAT32> = SIZE::SIZE_4;

    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::UINT64 > = SIZE::SIZE_8;
    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::INT64  > = SIZE::SIZE_8;
    template <>
    constexpr SIZE type_alignment<FIELD_TYPE::FLOAT64> = SIZE::SIZE_8;
}



[[nodiscard]] constexpr SIZE get_size_size (uint8_t /*unused*/) {
    return SIZE::SIZE_1;
}
[[nodiscard]] constexpr SIZE get_size_size (uint16_t size) {
    if (size <= UINT8_MAX) return SIZE::SIZE_1; 
    return SIZE::SIZE_2;
   
}
[[nodiscard]] constexpr SIZE get_size_size (uint32_t size) {
    if (size <= UINT8_MAX ) return SIZE::SIZE_1;
    if (size <= UINT16_MAX) return SIZE::SIZE_2;
    return SIZE::SIZE_4;
}
[[nodiscard]] constexpr SIZE get_size_size (uint64_t size) {
    if (size <= UINT8_MAX ) return SIZE::SIZE_1;
    if (size <= UINT16_MAX) return SIZE::SIZE_2;
    if (size <= UINT32_MAX) return SIZE::SIZE_4;
    return SIZE::SIZE_8;
}

struct LeafCounts {
private:
    uint64_t data {};

public:
    consteval LeafCounts () = default;
    constexpr explicit LeafCounts (AlignCounts counts) : data(std::bit_cast<uint64_t>(counts)) {}
    constexpr LeafCounts (uint16_t align1, uint16_t align2, uint16_t align4, uint16_t align8) : LeafCounts{{align1, align2, align4, align8}} {}
    constexpr explicit LeafCounts (uint64_t data) : data(data) {}
    constexpr explicit LeafCounts (SIZE size) : data(uint64_t{1} << (size.ordinal() * sizeof(uint16_t) * 8)) {}

    constexpr LeafCounts& operator += (this LeafCounts& self, const LeafCounts& other) {
        self.data += other.data;
        return self;
    }

    [[nodiscard]] constexpr LeafCounts operator + (const LeafCounts& other) const { return LeafCounts{data + other.data}; }

    [[nodiscard]] constexpr bool empty () const { return data == 0; }

    [[nodiscard]] constexpr AlignCounts counts () const { return std::bit_cast<AlignCounts>(data); }

    [[nodiscard]] constexpr uint16_t total () const { return counts().total(); }

    [[nodiscard]] static consteval LeafCounts zero () { return LeafCounts{AlignCounts::zero()}; }
    [[nodiscard]] static constexpr LeafCounts of (uint16_t value) { return LeafCounts{AlignCounts::of(value)}; }

    template <SIZE size>
    requires (size != SIZE::SIZE_0)
    [[nodiscard]] static consteval LeafCounts from_size () {
        return LeafCounts{size};
    }

    template <SIZE limit = SIZE::SIZE_8>
    [[nodiscard]] constexpr SIZE largest_align () const {
        return counts().largest_align<limit>();
    }
};

struct StructDefinition;
struct EnumDefinition;

struct IdentifiedDefinition {
    KEYWORDS keyword;

    struct alignas(8) Data {
        friend IdentifiedDefinition;
        std::string_view name;

    private:
        [[nodiscard]] const StructDefinition& as_struct () const;
        [[nodiscard]] const EnumDefinition& as_enum () const;
    };

    template <typename Derived, typename SpecificData, KEYWORDS keyword>
    struct Base : Data {
        friend Derived;
        
        SpecificData data;
    private:
        constexpr Base() = default;

    public:
        [[nodiscard]] static auto create(Buffer &buffer, const std::string_view name) {
            __CreateExtendedResult<Derived, IdentifiedDefinition> created = __create_extended<Derived, IdentifiedDefinition>(buffer);
            *buffer.get(created.base) = IdentifiedDefinition{keyword};
            buffer.get(created.extended)->name = name;
            return created;
        }
    };

private:
    [[nodiscard]] const Data& data () const;

public:
    template <typename Visitor, typename... Args>
    [[nodiscard]] decltype(auto) visit (Visitor&& visitor, Args&&... args) const {
        if (keyword == KEYWORDS::STRUCT) {
            const StructDefinition& struct_definition = data().as_struct();
            return std::forward<Visitor>(visitor).on_struct(struct_definition, std::forward<Args>(args)...);
        }
        if (keyword == KEYWORDS::ENUM) {
            const EnumDefinition& enum_defintion = data().as_enum();
            return std::forward<Visitor>(visitor).on_enum(enum_defintion, std::forward<Args>(args)...);
        }
        std::unreachable();
    }
};

struct IdentifiedType;
struct FixedStringType;
struct StringType;
struct ArrayType;

struct Type {
private:
    FIELD_TYPE type;

public:
    constexpr explicit Type(const FIELD_TYPE type) : type(type) {}

private:
    [[nodiscard]] const IdentifiedType& as_identifier () const;
    [[nodiscard]] const FixedStringType& as_fixed_string () const;
    [[nodiscard]] const StringType& as_string () const;
    [[nodiscard]] ArrayType& as_array () const;
    [[nodiscard]] auto& as_fixed_variant () const;
    [[nodiscard]] auto& as_packed_variant () const;
    [[nodiscard]] auto& as_dynamic_variant () const;

public:
    template <typename NextTypeT, typename ValueT = void>
    requires (!std::is_rvalue_reference_v<ValueT>)
    struct VisitResult {
        static constexpr bool value_is_ref = std::is_lvalue_reference_v<ValueT>;

        static_assert(
            value_is_ref || !std::is_const_v<ValueT>,
            "Non-reference members shouldn't be const"
        );

        using cont_next_type_t = const NextTypeT;
        using value_t = ValueT;

        constexpr VisitResult (cont_next_type_t& next_type, ValueT value) requires(value_is_ref)
            : next_type(next_type), value(value) {
            static_warn("Using reference type member");
        }
        
        constexpr VisitResult (cont_next_type_t& next_type, const ValueT& value) requires(!value_is_ref)
            : next_type(next_type), value(value) {}

        constexpr VisitResult (cont_next_type_t& next_type, ValueT&& value) requires(!value_is_ref)
            : next_type(next_type), value(std::move(value)) {}
        
        cont_next_type_t& next_type;
        ValueT value;
    };

    template <typename NextTypeT>
    struct VisitResult<NextTypeT, void> {
        using cont_next_type_t = const NextTypeT;
        using value_t = void;
        constexpr explicit VisitResult (cont_next_type_t& next_type)
            : next_type(next_type) {}
        
        cont_next_type_t& next_type;
    };

    template <typename VisitorT, typename... ArgsT>
    [[nodiscard]] std::remove_cvref_t<VisitorT>::result_t visit (VisitorT&& visitor, ArgsT&&... args) const;

    template <typename Visitor, typename Buffer, typename... Args>
    [[nodiscard]] decltype(auto) try_visit_identifier (Visitor&& visitor, Buffer&& buffer, Args&&... args) const;

    template <typename T>
    [[nodiscard]] inline T& skip () const;
};


template <typename T>
[[nodiscard, gnu::always_inline]] inline T* get_extended_type(auto* that) {
    return get_extended<T, Type>(that);
}


struct FixedStringType {
    friend Type;

    static void create (Buffer &buffer, uint32_t length) {
        auto [extended, base] = create_extended<FixedStringType, Type>(buffer);
        *base = Type{FIELD_TYPE::STRING_FIXED};
        *extended = {
            length,
            get_size_size(length)
        };
    }

    uint32_t length;
    SIZE length_size;

private:
    template <typename T>
    [[nodiscard]] T& after () const {
        return *estd::ptr_cast<T>(this + 1);
    }
};
[[nodiscard]] inline const FixedStringType& Type::as_fixed_string () const {
    return *get_extended_type<const FixedStringType>(this);
}

struct StringType {
    friend Type;

    static void create (Buffer &buffer, uint32_t min_length, SIZE stored_size_size, SIZE size_size) {
        auto [extended, base] = create_extended<StringType, Type>(buffer);
        *base = Type{STRING};
        *extended = {
            min_length,
            stored_size_size,
            size_size
        };
    }
    uint32_t min_length;
    SIZE stored_size_size;
    SIZE size_size;

private:
    template <typename T>
    [[nodiscard]] T& after () const {
        return *estd::ptr_cast<T>(this + 1);
    }
};
[[nodiscard]] inline const StringType& Type::as_string () const {
    return *get_extended_type<const StringType>(this);
}


using IdentifedDefinitionIndex = Buffer::Index<const IdentifiedDefinition>;
struct IdentifiedType {
    friend Type;

    static void create (Buffer &buffer, IdentifedDefinitionIndex identifier_idx) {
        auto [extended, base] = create_extended<IdentifiedType, Type>(buffer);
        *base = Type{IDENTIFIER};
        *extended = IdentifiedType{identifier_idx};
    }

private:
    IdentifedDefinitionIndex identifier_idx;

    constexpr explicit IdentifiedType(IdentifedDefinitionIndex idx) : identifier_idx(idx) {};

public:
    [[nodiscard]] const IdentifiedDefinition& get(const ReadOnlyBuffer& buffer) const {
        return *buffer.get(identifier_idx);
    }

    [[nodiscard]] const IdentifiedDefinition& get(const Buffer& buffer) const {
        return *buffer.get(identifier_idx);
    }

private:
    template <typename T>
    [[nodiscard]] T& after () const {
        return *estd::ptr_cast<T>(this + 1);
    }
};
[[nodiscard]] inline const IdentifiedType& Type::as_identifier () const {
    return *get_extended_type<const IdentifiedType>(this);
}

struct ArrayType {

    [[nodiscard]] static auto create (Buffer &buffer) {
        return __create_extended<ArrayType, Type>(buffer);
    }

    LeafCounts level_fixed_leafs;
    uint32_t length;
    uint16_t pack_info_base_idx;
    SIZE stored_size_size;
    SIZE size_size;

    [[nodiscard]] const Type& inner_type () const {
        return *estd::ptr_cast<const Type>(this + 1);
    }

};
[[nodiscard]] inline ArrayType& Type::as_array () const {
    return *get_extended_type<ArrayType>(this);
}



template <typename TypeMetaT>
struct VariantTypeBase {
    friend Type;
    
    [[nodiscard]] static auto create (Buffer &buffer) {
        return __create_extended<VariantTypeBase, Type, TypeMetaT>(buffer);
    }

    uint64_t min_byte_size;                 // Minimum byte size of the variant (used for size getter)
    Buffer::index_t type_metas_offset;      // Offset from head of this to the head of type_metas
    uint16_t variant_count;                 // Count of variants
    uint16_t total_fixed_leafs;             // Count of nested and non-nested fixed sized leafs
    uint16_t total_var_leafs;               // Count of nested and non-nested variable sized leafs
    SIZE stored_size_size;                  // Size of the stored size
    SIZE size_size;                         // Size of the size

    [[nodiscard]] const TypeMetaT* type_metas () const {
        return reinterpret_cast<const TypeMetaT*>(estd::ptr_cast<const uint8_t>(this) + type_metas_offset);
    }

    [[nodiscard]] const Type& first_variant() const {
        return *estd::ptr_cast<const Type>(this + 1);
    }
private:
    template <typename T>
    [[nodiscard]] T& after () const {
        return *estd::ptr_cast<T>(type_metas() + variant_count);
    }
};


struct FixedVariantTypeMeta {
    LeafCounts level_fixed_leafs;   // Counts of non-nested fixed sized leafs
    uint16_t level_fixed_variants;  // Counts of non-nested fixed variant fields
    uint16_t level_fixed_arrays;
    //LeafCounts variant_field_counts;
};

struct DynamicVariantTypeMeta {
    LeafCounts level_fixed_leafs;
    LeafCounts var_leaf_counts;
    uint16_t level_fixed_variants;
    uint16_t level_fixed_arrays;
    //LeafCounts variant_field_counts;
    uint16_t level_size_leafs;
};

using FixedVariantType = VariantTypeBase<FixedVariantTypeMeta>;
[[nodiscard]] inline auto& Type::as_fixed_variant () const {
    return *get_extended_type<FixedVariantType>(this);
}

using PackedVariantType = VariantTypeBase<FixedVariantTypeMeta>;
[[nodiscard]] inline auto& Type::as_packed_variant () const {
    return *get_extended_type<PackedVariantType>(this);
}

using DynamicVariantType = VariantTypeBase<DynamicVariantTypeMeta>;
[[nodiscard]] inline auto& Type::as_dynamic_variant () const {
    return *get_extended_type<DynamicVariantType>(this);
}


struct StructField {

    struct Data {
        std::string_view name;

        [[nodiscard]] const Type& type () const {
            return *estd::ptr_cast<const Type>(this + 1);
        }
    };

    [[nodiscard]] const Data& data () const {
        // The object of type Data can be found at its next natural alignment.
        return *get_padded<const Data>(this);
    }
};

struct EnumField {
    std::string_view name;

    struct Value {
        uint64_t value;
        bool is_negative;

        constexpr Value (uint64_t value, bool is_negative) : value(value), is_negative(is_negative) {}

        [[nodiscard]] static constexpr Value create (int64_t value) {
            if (value < 0) {
                return {static_cast<uint64_t>(-value), true}; 
            }
            return {static_cast<uint64_t>(value), false};
        }

        constexpr explicit Value (int64_t value) : Value{create(value)} {}
        
        constexpr void increment () {
            if (is_negative) {
                if (value == 1) {
                    is_negative = false;
                } /*else if (value == 0) {
                    INTERNAL_ERROR("[set_member_value] value would underflow");
                }*/ // this state should never happen since we dont call set_member_value with "-0"
                value--;
            } else {
                if (value == std::numeric_limits<uint64_t>::max()) {
                    console.error("[EnumValue::next] value would overflow");
                }
                value++;
            }
        }

        static constexpr Value intitial () {
            return Value{-1};
        }
    } value;
};

struct StructDefinitionData {
    LeafCounts level_fixed_leafs;
    LeafCounts var_leaf_counts;
    //LeafCounts variant_field_counts;
    // LeafSizes fixed_leaf_sizes;
    uint64_t min_byte_size;
    uint64_t max_byte_size;
    uint16_t level_size_leafs;
    uint16_t level_fixed_variants;
    uint16_t level_fixed_arrays;
    uint16_t level_variant_fields;
    uint16_t sublevel_fixed_leafs;
    uint16_t total_variant_var_leafs;
    uint16_t field_count;
    uint16_t pack_count;
    SIZE max_alignment;
};

struct StructDefinition : IdentifiedDefinition::Base<StructDefinition, StructDefinitionData, KEYWORDS::STRUCT> {
    static StructField::Data& reserve_field(Buffer &buffer) {
        // Since we can create a Data object any time the current buffer position might not be aligned, so we have to ensure alignment.
        return *create_padded<StructField::Data>(buffer);
    }
private:
    [[nodiscard]] const StructField* first_field() const {
        return estd::ptr_cast<const StructField>(this + 1);
    }
public:
    template <typename VisitorT>
    void visit (VisitorT&& visitor) const {
        const StructField* field = first_field();
        for (uint16_t i = 0; i < data.field_count; i++) {
            field = &visitor(field->data());
        }
    }

    template <typename VisitorT>
    void visit_uninitialized (VisitorT&& visitor, const uint16_t field_count) const {
        const StructField* field = first_field();
        for (uint16_t i = 0; i < field_count; i++) {
            field = &visitor(field->data());
        }
    }
};

struct EnumDefinitionData {
    uint16_t field_count;
    SIZE type_size;
};

struct EnumDefinition : IdentifiedDefinition::Base<EnumDefinition, EnumDefinitionData, KEYWORDS::ENUM> {
    static void add_field (Buffer &buffer, const EnumField& field) {
        // We only create one EnumField after the other, so the alignment is always correct.
        static_assert(alignof(EnumDefinition) >= alignof(EnumField));
        *buffer.get_next_aligned<EnumField>() = field;
    }

    std::span<EnumField> fields() {
        return {estd::ptr_cast<EnumField>(this + 1), data.field_count};
    }
};


const IdentifiedDefinition::Data& IdentifiedDefinition::data () const {
    // The object of type Data is created when classes, which are derived from Data, are created.
    return *get_extended<const Data, IdentifiedDefinition>(this);
}

[[nodiscard]] inline const StructDefinition& IdentifiedDefinition::Data::as_struct () const {
    static_assert(alignof(IdentifiedDefinition::Data) >= alignof(StructDefinition));
    static_assert(std::is_base_of_v<IdentifiedDefinition::Data, StructDefinition>);
    return *static_cast<const StructDefinition*>(this);
}
[[nodiscard]] inline const EnumDefinition& IdentifiedDefinition::Data::as_enum () const {
    static_assert(alignof(IdentifiedDefinition::Data) >= alignof(EnumDefinition));
    static_assert(std::is_base_of_v<IdentifiedDefinition::Data, EnumDefinition>);
    return *static_cast<const EnumDefinition*>(this);
}

using IdentifierMap = boost::unordered::unordered_flat_map<std::string_view, IdentifedDefinitionIndex>;


template <typename T>
[[nodiscard]] inline T& Type::skip () const {
    switch (type) {
        case STRING_FIXED:      return as_fixed_string().after<T>();
        case STRING:            return as_string().after<T>();
        case ARRAY_FIXED:
        case ARRAY:             return as_array().inner_type().skip<T>();
        case FIXED_VARIANT:     return as_fixed_variant().after<T>();
        case PACKED_VARIANT:    return as_packed_variant().after<T>();
        case DYNAMIC_VARIANT:   return as_dynamic_variant().after<T>();
        case IDENTIFIER:        return as_identifier().after<T>();
        case BOOL:
        case UINT8:
        case UINT16:
        case UINT32:
        case UINT64:
        case INT8:
        case INT16:
        case INT32:
        case INT64:
        case FLOAT32:
        case FLOAT64:           return *estd::ptr_cast<T>(this + 1);
        default:
            std::unreachable();
    }
}

template <typename VisitorT, typename... ArgsT>
[[nodiscard]] inline std::remove_cvref_t<VisitorT>::result_t Type::visit (VisitorT&& visitor, ArgsT&&... args) const {
    using visitor_t = std::remove_cvref_t<VisitorT>;
    using result_t = visitor_t::result_t;
    using const_next_type_t = const visitor_t::next_type_t;
    static constexpr bool no_value = std::is_same_v<typename result_t::value_t, void>;

    switch (type) {
        case FIELD_TYPE::BOOL: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_bool(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_bool(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::UINT8: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_uint8(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_uint8(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::UINT16: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_uint16(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_uint16(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::UINT32: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_uint32(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_uint32(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::UINT64: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_uint64(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_uint64(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::INT8: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_int8(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_int8(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::INT16: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_int16(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_int16(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::INT32: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_int32(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_int32(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::INT64: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_int64(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_int64(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::FLOAT32: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_float32(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_float32(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::FLOAT64: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_float64(std::forward<ArgsT>(args)...);
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{*estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_float64(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::STRING_FIXED: {
            const FixedStringType& fixed_string_type = as_fixed_string();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_fixed_string(fixed_string_type);
                return result_t{fixed_string_type.after<const_next_type_t>()};
            } else {
                return result_t{fixed_string_type.after<const_next_type_t>(),
                    std::forward<VisitorT>(visitor).on_fixed_string(fixed_string_type, std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::STRING: {
            const StringType& string_type = as_string();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_string(string_type);
                return result_t{string_type.after<const_next_type_t>()};
            } else {
                return result_t{string_type.after<const_next_type_t>(),
                    std::forward<VisitorT>(visitor).on_string(string_type, std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::ARRAY_FIXED: {
            return std::forward<VisitorT>(visitor).on_fixed_array(as_array(), std::forward<ArgsT>(args)...);
        }
        case FIELD_TYPE::ARRAY: {
            return std::forward<VisitorT>(visitor).on_array(as_array(), std::forward<ArgsT>(args)...);
        }
        case FIELD_TYPE::FIXED_VARIANT: {
            FixedVariantType& fixed_variant_type = as_fixed_variant();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_fixed_variant(fixed_variant_type, std::forward<ArgsT>(args)...);
                return result_t{fixed_variant_type.after<const_next_type_t>()};
            } else {
                return result_t{fixed_variant_type.after<const_next_type_t>(),
                    std::forward<VisitorT>(visitor).on_fixed_variant(fixed_variant_type, std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::PACKED_VARIANT: {
            PackedVariantType& packed_variant_type = as_packed_variant();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_packed_variant(packed_variant_type, std::forward<ArgsT>(args)...);
                return result_t{packed_variant_type.after<const_next_type_t>()};
            } else {
                return result_t{packed_variant_type.after<const_next_type_t>(),
                    std::forward<VisitorT>(visitor).on_packed_variant(packed_variant_type, std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::DYNAMIC_VARIANT: {
            DynamicVariantType& dynamic_variant_type = as_dynamic_variant();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_dynamic_variant(dynamic_variant_type, std::forward<ArgsT>(args)...);
                return result_t{dynamic_variant_type.after<const_next_type_t>()};
            } else {
                return result_t{dynamic_variant_type.after<const_next_type_t>(),
                    std::forward<VisitorT>(visitor).on_dynamic_variant(dynamic_variant_type, std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::IDENTIFIER: {
            const IdentifiedType& identifier_type = as_identifier();
            const IdentifiedDefinition& identified = identifier_type.get(visitor.get_ast_buffer());
            if constexpr (no_value) {
                identified.visit(std::forward<VisitorT>(visitor), std::forward<ArgsT>(args)...);
                return result_t{identifier_type.after<const_next_type_t>()};
            } else {
                return result_t{identifier_type.after<const_next_type_t>(),
                    identified.visit(std::forward<VisitorT>(visitor), std::forward<ArgsT>(args)...)};
            }
        }
        default: {
            std::unreachable();
        }
    }
}

template <typename Visitor, typename Buffer, typename... Args>
[[nodiscard]] decltype(auto) Type::try_visit_identifier (Visitor&& visitor, Buffer&& buffer, Args&&... args) const {
        if (type == FIELD_TYPE::IDENTIFIER) {
            const IdentifiedType& identifier_type = as_identifier();
            const IdentifiedDefinition& identified = identifier_type.get(buffer);
            return identified.visit(std::forward<Visitor>(visitor), std::forward<Buffer>(buffer), std::forward<Args>(args)...);
        }
        return std::forward<Visitor>(visitor).on_fail(std::forward<Buffer>(buffer), std::forward<Args>(args)...);
    }

}
