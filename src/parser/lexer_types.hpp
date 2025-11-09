#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <gsl/util>
#include <type_traits>
#include <string_view>
#include <utility>
#include <boost/unordered/unordered_flat_map.hpp>

#include "../container/memory.hpp"
#include "./memory_helpers.hpp"
#include "../estd/empty.hpp"
#include "../estd/enum.hpp"

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
    struct size_helper;
}

struct SIZE : estd::ENUM_CLASS<uint8_t> {
    friend struct size_helper;

    using ENUM_CLASS::ENUM_CLASS;
    
    static const SIZE SIZE_1;
    static const SIZE SIZE_2;
    static const SIZE SIZE_4;
    static const SIZE SIZE_8;
    static const SIZE SIZE_0;

    [[nodiscard]] constexpr value_t byte_size () const {
        return value_t{1} << value;
    }

private:
    static void next_smaller_is_invalid_for_this_size () {}
    static void next_bigger_is_invalid_for_this_size () {}

public:
    [[nodiscard]] consteval SIZE next_smaller () const {
        if (value == SIZE_0 || value == SIZE_1) {
            next_smaller_is_invalid_for_this_size();
        }
        return SIZE{gsl::narrow_cast<value_t>(value - 1)};
    };

    [[nodiscard]] consteval SIZE next_bigger () const {
        if (value == SIZE_0 || value == SIZE_8) {
            next_bigger_is_invalid_for_this_size();
        }
        return SIZE{gsl::narrow_cast<value_t>(value + 1)};
    };
};


constexpr SIZE SIZE::SIZE_1{0};
constexpr SIZE SIZE::SIZE_2{1};
constexpr SIZE SIZE::SIZE_4{2};
constexpr SIZE SIZE::SIZE_8{3};
constexpr SIZE SIZE::SIZE_0{0xff};

namespace {
    struct size_helper {
        template <SIZE size>
        requires (size != SIZE::SIZE_0 && size != SIZE::SIZE_1)
        static constexpr SIZE next_smaller_size {size - 1};

        template <SIZE size>
        requires (size != SIZE::SIZE_0 && size != SIZE::SIZE_8)
        static constexpr SIZE next_bigger_size {size + 1};
    };
}

// namespace {
//     template <SIZE size>
//     consteval SIZE next_smaller_size_ () {
//         if constexpr (size == SIZE::SIZE_8) {
//             return SIZE::SIZE_4;
//         } else if constexpr (size == SIZE::SIZE_4) {
//             return SIZE::SIZE_2;
//         } else if constexpr (size == SIZE::SIZE_2) {
//             return SIZE::SIZE_1;
//         }
//     }

//     template <SIZE size>
//     consteval SIZE next_bigger_size_ () {
//         if constexpr (size == SIZE::SIZE_4) {
//             return SIZE::SIZE_8;
//         } else if constexpr (size == SIZE::SIZE_2) {
//             return SIZE::SIZE_4;
//         } else if constexpr (size == SIZE::SIZE_1) {
//             return SIZE::SIZE_2;
//         }
//     }
// }

template <SIZE size>
requires (size != SIZE::SIZE_0 && size != SIZE::SIZE_1)
constexpr SIZE next_smaller_size = size_helper::next_smaller_size<size>;

template <SIZE size>
requires (size != SIZE::SIZE_0 && size != SIZE::SIZE_8)
constexpr SIZE next_bigger_size = size_helper::next_bigger_size<size>;

template <typename T>
struct AlignMembersBase {
    T align1;
    T align2;
    T align4;
    T align8;

    constexpr AlignMembersBase() = default;
    constexpr AlignMembersBase (const T& align1, const T& align2, const T& align4, const T& align8)
    : align1(align1), align2(align2), align4(align4), align8(align8) {}

    #define ALIGN_MEMBER_GET_CT_ARG(RETURN_TYPE, CONST_ATTR)            \
    template <lexer::SIZE size>                                         \
    [[nodiscard]] constexpr CONST_ATTR RETURN_TYPE& get () CONST_ATTR { \
        if constexpr (size == lexer::SIZE::SIZE_8) {                    \
            return align8;                                              \
        } else if constexpr (size == lexer::SIZE::SIZE_4) {             \
            return align4;                                              \
        } else if constexpr (size == lexer::SIZE::SIZE_2) {             \
            return align2;                                              \
        } else if constexpr (size == lexer::SIZE::SIZE_1) {             \
            return align1;                                              \
        } else {                                                        \
            static_assert(false, "Invalid size");                       \
        }                                                               \
    }

    #define ALIGN_MEMBER_GET_RT_ARG(RETURN_TYPE, CONST_ATTR)                            \
    [[nodiscard]] constexpr CONST_ATTR RETURN_TYPE& get (lexer::SIZE size) CONST_ATTR { \
        switch (size) {                                                                 \
            case SIZE::SIZE_1: return align1;                                           \
            case SIZE::SIZE_2: return align2;                                           \
            case SIZE::SIZE_4: return align4;                                           \
            case SIZE::SIZE_8: return align8;                                           \
            default: std::unreachable();                                                \
        }                                                                               \
    }

    ALIGN_MEMBER_GET_CT_ARG(T, )
    ALIGN_MEMBER_GET_CT_ARG(T, const)
    ALIGN_MEMBER_GET_RT_ARG(T, )
    ALIGN_MEMBER_GET_RT_ARG(T, const)

    #undef ALIGN_MEMBER_GET_CT_ARG
    #undef ALIGN_MEMBER_GET_RT_ARG
};

template <lexer::SIZE alignemnt, typename T>
[[nodiscard]] constexpr auto& get_align_member (T& t) {
    if constexpr (alignemnt == lexer::SIZE::SIZE_8) {
        return t.align8;
    } else if constexpr (alignemnt == lexer::SIZE::SIZE_4) {
        return t.align4;
    } else if constexpr (alignemnt == lexer::SIZE::SIZE_2) {
        return t.align2;
    } else if constexpr (alignemnt == lexer::SIZE::SIZE_1) {
        return t.align1;
    } else {
        static_assert(false, "Invalid size");
    }
}

template <FIELD_TYPE field_type>
[[nodiscard]] consteval SIZE get_type_alignment () {
    if constexpr (
           field_type == FIELD_TYPE::UINT8
        || field_type == FIELD_TYPE::INT8
        || field_type == FIELD_TYPE::BOOL
    ) {
        return SIZE::SIZE_1;
    } else if constexpr (
           field_type == FIELD_TYPE::UINT16
        || field_type == FIELD_TYPE::INT16
    ) {
        return SIZE::SIZE_2;
    } else if constexpr (
           field_type == FIELD_TYPE::UINT32
        || field_type == FIELD_TYPE::INT32
        || field_type == FIELD_TYPE::FLOAT32
    ) {
        return SIZE::SIZE_4;
    } else if constexpr (
           field_type == FIELD_TYPE::UINT64
        || field_type == FIELD_TYPE::INT64
        || field_type == FIELD_TYPE::FLOAT64
    ) {
        return SIZE::SIZE_8;
    } else {
        static_assert(false, "Can't get alignment of this field type.");
    }
}
template <FIELD_TYPE field_type>
[[nodiscard]] consteval uint64_t get_type_size () {
    return get_type_alignment<field_type>().byte_size();
}

template <std::integral T>
[[nodiscard]] constexpr T last_multiple (T value, SIZE base) {
    return value & ~(static_cast<T>(base.byte_size()) - 1);
}

template <std::integral T>
[[nodiscard]] constexpr T last_multiple (T value, std::type_identity_t<T> base) {
    static_warn("base must be a power of 2");
    return value & ~(base - 1);
}


template <std::integral T>
[[nodiscard]] constexpr T next_multiple (T value, SIZE base) {
    T mask = static_cast<T>(base.byte_size()) - 1;
    return (value + mask) & ~mask;
}

template <std::integral T>
[[nodiscard]] constexpr T next_multiple (T value, std::type_identity_t<T> base) {
    static_warn("base must be a power of 2");
    T mask = base - 1;
    return (value + mask) & ~mask;
}

[[nodiscard]] constexpr SIZE get_size_size (uint8_t /*unused*/) {
    return SIZE::SIZE_1;
}
[[nodiscard]] constexpr SIZE get_size_size (uint16_t size) {
    if (size <= UINT8_MAX) {
        return SIZE::SIZE_1;
    } else {
        return SIZE::SIZE_2;
    }
}
[[nodiscard]] constexpr SIZE get_size_size (uint32_t size) {
    if (size <= UINT8_MAX) {
        return SIZE::SIZE_1;
    } else if (size <= UINT16_MAX) {
        return SIZE::SIZE_2;
    } else {
        return SIZE::SIZE_4;
    }
}
[[nodiscard]] constexpr SIZE get_size_size (uint64_t size) {
    if (size <= UINT8_MAX) {
        return SIZE::SIZE_1;
    } else if (size <= UINT16_MAX) {
        return SIZE::SIZE_2;
    } else if (size <= UINT32_MAX) {
        return SIZE::SIZE_4;
    } else {
        return SIZE::SIZE_8;
    }
}

struct Range {
    uint32_t min;
    uint32_t max;
};

union LeafCounts {

    struct Counts : AlignMembersBase<uint16_t> {
        using AlignMembersBase::AlignMembersBase;

        [[nodiscard]] constexpr uint16_t total () const {
            return align1 + align2 + align4 + align8;
        }

        [[nodiscard]] constexpr SIZE largest_align () const {
            if (align8 != 0) return SIZE::SIZE_8;
            if (align4 != 0) return SIZE::SIZE_4;
            if (align2 != 0) return SIZE::SIZE_2;
            return SIZE::SIZE_1;
        }

        [[nodiscard]] static consteval Counts zero () { return {0, 0, 0, 0}; }
        [[nodiscard]] static constexpr Counts of (uint16_t value) { return {value, value, value, value}; }
    } counts;
    uint64_t as_uint64;

    static_assert(offsetof(Counts, align1) == 0
               && offsetof(Counts, align2) == 2
               && offsetof(Counts, align4) == 4
               && offsetof(Counts, align8) == 6);

    constexpr LeafCounts () = default;
    constexpr explicit LeafCounts (Counts counts) : counts(counts) {}
    constexpr LeafCounts (uint16_t align1, uint16_t align2, uint16_t align4, uint16_t align8) : counts{align1, align2, align4, align8} {}
    constexpr explicit LeafCounts (uint64_t as_uint64) : as_uint64(as_uint64) {}
    constexpr explicit LeafCounts (SIZE size) : as_uint64(uint64_t{1} << (size.value * sizeof(uint16_t) * 8)) {}

    constexpr void operator += (const LeafCounts& other) { as_uint64 += other.as_uint64; }
    [[nodiscard]] constexpr LeafCounts operator + (const LeafCounts& other) const { return LeafCounts{as_uint64 + other.as_uint64}; }
    [[nodiscard]] constexpr uint16_t total () const { return counts.total(); }
    [[nodiscard]] constexpr bool empty () const { return as_uint64 == 0; }

    [[nodiscard]] static consteval LeafCounts zero () { return LeafCounts{Counts::zero()}; }
    [[nodiscard]] static constexpr LeafCounts of (uint16_t value) { return LeafCounts{Counts::of(value)}; }

    template <SIZE size>
    [[nodiscard]] static consteval LeafCounts from_size () {
        if constexpr (size == SIZE::SIZE_1) {
            return {1, 0, 0, 0};
        } else if constexpr (size == SIZE::SIZE_2) {
            return {0, 1, 0, 0};
        } else if constexpr (size == SIZE::SIZE_4) {
            return {0, 0, 1, 0};
        } else if constexpr (size == SIZE::SIZE_8) {
            return {0, 0, 0, 1};
        } else {
            static_assert(false, "Invalid size");
        }
    }

    [[nodiscard]] constexpr SIZE largest_align () const {
        return counts.largest_align();
    }
};


// struct uint256_t {
//     uint64_t data[4];

//     // 0 -> most significant, 3 -> least significant

//     #define ASSIGN(op) \
//     [[nodiscard]] uint256_t& operator op (const uint256_t& other) { \
//         data[0] op other.data[0]; \
//         data[1] op other.data[1]; \
//         data[2] op other.data[2]; \
//         data[3] op other.data[3]; \
//         return *this; \
//     }

//     ASSIGN(+=)
//     ASSIGN(-=)
//     ASSIGN(*=)
//     ASSIGN(/=)

//     #undef ASSIGN

//     #define ARITHMETIC(op) \
//     uint256_t operator op (const uint256_t& other) const { \
//         return { \
//             data[0] op other.data[0], \
//             data[1] op other.data[1], \
//             data[2] op other.data[2], \
//             data[3] op other.data[3] \
//         }; \
//     }

//     // Inocrrect since we dont carray data
//     ARITHMETIC(+)
//     ARITHMETIC(-)
//     ARITHMETIC(*)
//     ARITHMETIC(/)

//     #undef ARITHMETIC

//     #define COMPARE(op) \
//     bool operator op (const uint256_t& other) const { \
//         if (data[0] op other.data[0]) return true; \
//         if (data[1] op other.data[1]) return true; \
//         if (data[2] op other.data[2]) return true; \
//         if (data[3] op other.data[3]) return true; \
//         return false; \
//     }

//     COMPARE(==)
//     COMPARE(!=)
//     COMPARE(>)
//     COMPARE(<)
//     COMPARE(>=)
//     COMPARE(<=)

//     #undef COMPARE
// }; */

struct LeafSizes : AlignMembersBase<uint64_t> {
    using AlignMembersBase::AlignMembersBase;
    constexpr explicit LeafSizes (const LeafCounts::Counts& counts) : AlignMembersBase{counts.align1, counts.align2, counts.align4, counts.align8} {}

    constexpr LeafSizes& operator += (const LeafSizes& other) {
        align1 += other.align1;
        align2 += other.align2;
        align4 += other.align4;
        align8 += other.align8;
        return *this;
    }

    [[nodiscard]] constexpr LeafSizes operator + (const LeafSizes& other) const {
        return {
            align1 + other.align1,
            align2 + other.align2,
            align4 + other.align4,
            align8 + other.align8
        };
    }
    [[nodiscard]] constexpr LeafSizes operator * (const uint32_t factor) const {
        return {
            align1 * factor,
            align2 * factor,
            align4 * factor,
            align8 * factor
        };
    }

    [[nodiscard]] constexpr bool operator > (const LeafSizes& other) const {
        if (align8 > other.align8) return true;
        if (align4 > other.align4) return true;
        if (align2 > other.align2) return true;
        if (align1 > other.align1) return true;
        return false;
    }

    [[nodiscard]] constexpr bool operator < (const LeafSizes& other) const {
        if (align8 < other.align8) return true;
        if (align4 < other.align4) return true;
        if (align2 < other.align2) return true;
        if (align1 < other.align1) return true;
        return false;
    }

    [[nodiscard]] constexpr uint64_t total () const { return align1 + align2 + align4 + align8; }

    [[nodiscard]] constexpr bool empty () const {
        return total() == 0;
    }

    template <lexer::SIZE... sizes>
    [[nodiscard]] constexpr uint64_t sum_sizes () {
        return (... + get<sizes>());
    }

    [[nodiscard]] static consteval LeafSizes zero () { return {0, 0, 0, 0}; }

    template <SIZE size>
    [[nodiscard]] static constexpr LeafSizes from_size (uint32_t count) {
        if constexpr (size == SIZE::SIZE_1) {
            return {uint64_t{1} * count, 0, 0, 0};
        } else if constexpr (size == SIZE::SIZE_2) {
            return {0, uint64_t{2} * count, 0, 0};
        } else if constexpr (size == SIZE::SIZE_4) {
            return {0, 0, uint64_t{4} * count, 0};
        } else if constexpr (size == SIZE::SIZE_8) {
            return {0, 0, 0, uint64_t{8} * count};
        } else {
            static_assert(false, "Invalid size");
        }
    }
    template <SIZE size>
    [[nodiscard]] static consteval LeafSizes from_size () {
        return from_size<size>(1);
    }


    [[nodiscard]] static constexpr LeafSizes from_size (SIZE size, uint32_t count) {
        switch (size) {
            case SIZE::SIZE_1: return {uint64_t{1} * count, 0, 0, 0};
            case SIZE::SIZE_2: return {0, uint64_t{2} * count, 0, 0};
            case SIZE::SIZE_4: return {0, 0, uint64_t{4} * count, 0};
            case SIZE::SIZE_8: return {0, 0, 0, uint64_t{8} * count};
            default: std::unreachable();
        }
    }

    [[nodiscard]] static constexpr LeafSizes from_space_at_size (SIZE size, uint64_t space) {
        switch (size) {
            case SIZE::SIZE_1: return {space, 0, 0, 0};
            case SIZE::SIZE_2: return {0, space, 0, 0};
            case SIZE::SIZE_4: return {0, 0, space, 0};
            case SIZE::SIZE_8: return {0, 0, 0, space};
            default: std::unreachable();
        }
    }
};

template <SIZE size, bool as_const = true>
[[nodiscard, gnu::always_inline]] constexpr estd::conditional_const_t<as_const, uint64_t>& get_size (estd::conditional_const_t<as_const, LeafSizes>& sizes) {
    if constexpr (size == SIZE::SIZE_8) {
        return sizes.align8;
    } else if constexpr (size == SIZE::SIZE_4) {
        return sizes.align4;
    } else if constexpr (size == SIZE::SIZE_2) {
        return sizes.align2;
    } else {
        return sizes.align1;
    }
}

struct IdentifiedDefinition {
    KEYWORDS keyword;

    struct Data {
        std::string_view name;

        [[nodiscard]] const auto* as_struct () const;
        [[nodiscard]] const auto* as_enum () const;
    };

    [[nodiscard]] const Data* data () const;

};


struct Type {
    FIELD_TYPE type;

    [[nodiscard]] const auto* as_fixed_string () const;
    [[nodiscard]] const auto* as_string () const;
    [[nodiscard]] const auto* as_identifier () const;
    [[nodiscard]] const auto* as_array () const;
    [[nodiscard]] auto* as_fixed_variant () const;
    [[nodiscard]] auto* as_packed_variant () const;
    [[nodiscard]] auto* as_dynamic_variant () const;
};


template <typename T>
[[nodiscard, gnu::always_inline]] inline T* get_extended_type(auto* that) {
    return get_extended<T, Type>(that);
}


struct FixedStringType {
    static void create (Buffer &buffer, uint32_t length) {
        auto [extended, base] = create_extended<FixedStringType, Type>(buffer);
        base->type = FIELD_TYPE::STRING_FIXED;
        extended->length = length;
        extended->length_size = get_size_size(length);
    }

    uint32_t length;
    SIZE length_size;
};
[[nodiscard]] inline const auto* Type::as_fixed_string () const {
    return get_extended_type<const FixedStringType>(this);
}

struct StringType {
    static void create (Buffer &buffer, uint32_t min_length, SIZE stored_size_size, SIZE size_size) {
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
[[nodiscard]] inline const auto* Type::as_string () const {
    return get_extended_type<const StringType>(this);
}


using IdentifedDefinitionIndex = Buffer::Index<const IdentifiedDefinition>;
struct IdentifiedType {
    static void create (Buffer &buffer, IdentifedDefinitionIndex identifier_idx) {
        auto [extended, base] = create_extended<IdentifiedType, Type>(buffer);
        base->type = IDENTIFIER;
        extended->identifier_idx = identifier_idx;
    }

    IdentifedDefinitionIndex identifier_idx;
};
[[nodiscard]] inline const auto* Type::as_identifier () const {
    return get_extended_type<const IdentifiedType>(this);
}

struct ArrayType {

    [[nodiscard]] static auto create (Buffer &buffer) {
        return __create_extended<ArrayType, Type>(buffer);
    }

    uint32_t length;
    SIZE stored_size_size;
    SIZE size_size;

    [[nodiscard]] const Type* inner_type () const {
        static_assert(alignof(ArrayType) >= alignof(Type));
        return reinterpret_cast<const Type*>(this + 1);
    }

};
[[nodiscard]] inline const auto* Type::as_array () const {
    return get_extended_type<const ArrayType>(this);
}



template <typename TypeMeta_T>
struct VariantTypeBase {
    [[nodiscard]] static auto create (Buffer &buffer) {
        return __create_extended<VariantTypeBase, Type, TypeMeta_T>(buffer);
    }

    LeafSizes pack_sizes;               // Maximum byte sizes of fixed sized leafs over all variants
    uint64_t min_byte_size;             // Minimum byte size of the variant (used for size getter)
    uint16_t variant_count;             // Count of variants
    uint16_t total_fixed_leafs;         // Count of nested and non-nested fixed sized leafs
    uint16_t total_var_leafs;           // Count of nested and non-nested variable sized leafs
    SIZE stored_size_size;              // Size of the stored size
    SIZE size_size;                     // Size of the size

    [[nodiscard]] const TypeMeta_T* type_metas () const {
        static_assert(alignof(VariantTypeBase) >= alignof(TypeMeta_T));
        return get_padded<const TypeMeta_T>(this + 1);
    }

    [[nodiscard]] const Type* first_variant() const {
        // return reinterpret_cast<const Type*>(type_metas() + variant_count);
        static_assert(alignof(VariantTypeBase) >= alignof(Type) && alignof(TypeMeta_T) >= alignof(Type));
        return reinterpret_cast<const Type*>(reinterpret_cast<const uint8_t*>(type_metas()) + (variant_count * sizeof(TypeMeta_T)));
    }
};


struct FixedVariantTypeMeta {
    LeafCounts fixed_leaf_counts;   // Counts of non-nested fixed sized leafs
    uint16_t level_fixed_variants;  // Counts of non-nested fixedd variant fields
    //LeafCounts variant_field_counts;
};

struct DynamicVariantTypeMeta {
    LeafCounts fixed_leaf_counts;
    LeafCounts var_leaf_counts;
    uint16_t level_fixed_variants;
    //LeafCounts variant_field_counts;
    uint16_t level_size_leafs;
};

using FixedVariantType = VariantTypeBase<FixedVariantTypeMeta>;
[[nodiscard]] inline auto* Type::as_fixed_variant () const {
    return get_extended_type<FixedVariantType>(this);
}

using PackedVariantType = VariantTypeBase<FixedVariantTypeMeta>;
[[nodiscard]] inline auto* Type::as_packed_variant () const {
    return get_extended_type<PackedVariantType>(this);
}

using DynamicVariantType = VariantTypeBase<DynamicVariantTypeMeta>;
[[nodiscard]] inline auto* Type::as_dynamic_variant () const {
    return get_extended_type<DynamicVariantType>(this);
}


struct StructField {

    struct Data {
        std::string_view name;

        [[nodiscard]] const Type* type () const {
            static_assert(alignof(Data) >= alignof(Type));
            return reinterpret_cast<const Type*>(this + 1);
        }
    };

    [[nodiscard]] const Data* data () const {
        // The object of type Data can be found at its next natural alignment.
        return get_padded<const Data>(this);
    }
};

struct EnumField {
    std::string_view name;

    struct Value {
        constexpr explicit Value (int64_t value) {
            if (value < 0) {
                is_negative = true;
                this->value = -value;
            } else {
                is_negative = false;
                this->value = value;
            }
        }
        constexpr Value (uint64_t value, bool is_negative) : value(value), is_negative(is_negative) {}

        uint64_t value;
        bool is_negative;

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
                    logger::error("[EnumValue::next] value would overflow");
                }
                value++;
            }
        }

        static constexpr Value intitial () {
            return Value{-1};
        }
    } value;

    EnumField* next () {
        return this + 1;
    }
};

struct StructDefinition : IdentifiedDefinition::Data {
    LeafCounts fixed_leaf_counts;
    LeafCounts var_leaf_counts;
    //LeafCounts variant_field_counts;
    // LeafSizes fixed_leaf_sizes;
    uint64_t min_byte_size;
    uint64_t max_byte_size;
    uint16_t level_size_leafs;
    uint16_t level_fixed_variants;
    uint16_t level_variant_fields;
    uint16_t total_variant_fixed_leafs;
    uint16_t total_variant_var_leafs;
    uint16_t field_count;
    SIZE max_alignment;

    [[nodiscard]] static auto create(Buffer &buffer) {
        return __create_extended<StructDefinition, IdentifiedDefinition>(buffer);
    }

    static StructField::Data* reserve_field(Buffer &buffer) {
        // Since we can create a Data object any time the current buffer position might not be aligned, so we have to ensure alignment.
        return create_padded<StructField::Data>(buffer);
    }

    [[nodiscard]] const StructField* first_field() const {
        static_assert(alignof(StructDefinition) >= alignof(StructField));
        return reinterpret_cast<const StructField*>(this + 1);
    }
};

struct EnumDefinition : IdentifiedDefinition::Data {
    uint16_t field_count;
    SIZE type_size;

    [[nodiscard]] static auto create(Buffer &buffer) {
        return __create_extended<EnumDefinition, IdentifiedDefinition>(buffer);
    }

    static void add_field (Buffer &buffer, const EnumField& field) {
        // We only create one EnumField after the other, so the alignment is always correct.
        static_assert(alignof(EnumDefinition) >= alignof(EnumField));
        *buffer.get_next_aligned<EnumField>() = field;
    }

    EnumField* first_field() {
        static_assert(alignof(EnumDefinition) >= alignof(EnumField));
        return reinterpret_cast<EnumField*>(this + 1);
    }
};


const IdentifiedDefinition::Data* IdentifiedDefinition::data () const {
    // The object of type Data is created when classes, which are derived from Data, are created.
    return get_extended<const Data, IdentifiedDefinition>(this);
}

[[nodiscard]] inline const auto* IdentifiedDefinition::Data::as_struct () const {
    static_assert(alignof(IdentifiedDefinition::Data) >= alignof(StructDefinition));
    static_assert(std::is_base_of_v<IdentifiedDefinition::Data, StructDefinition>);
    return static_cast<const StructDefinition*>(this);
}
[[nodiscard]] inline const auto* IdentifiedDefinition::Data::as_enum () const {
    static_assert(alignof(IdentifiedDefinition::Data) >= alignof(EnumDefinition));
    static_assert(std::is_base_of_v<IdentifiedDefinition::Data, EnumDefinition>);
    return static_cast<const EnumDefinition*>(this);
}

using IdentifierMap = boost::unordered::unordered_flat_map<std::string_view, IdentifedDefinitionIndex>;


template <typename T>
inline const T* skip_type (const Type* type) {
    using U = const T;
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

template <typename T, typename TypeMeta_T>
inline const T* skip_variant_type (const VariantTypeBase<TypeMeta_T>* variant_type) {
    auto types_count = variant_type->variant_count;
    const Type* type = variant_type->first_variant();
    for (uint16_t i = 0; i < types_count; i++) {
        type = skip_type<Type>(type);
    }
    return reinterpret_cast<const T*>(type);
}

template <typename TypeT, typename ValueT>
requires (!std::is_rvalue_reference_v<ValueT>)
struct TypeVisitorResult;

template <typename TypeT>
struct TypeVisitorResult<TypeT, void> {
    using ConstTypeT = const TypeT;
    constexpr explicit TypeVisitorResult (ConstTypeT*&& next_type) : next_type(std::move(next_type)) {}
    
    ConstTypeT* next_type;
};

template <typename TypeT, typename ValueT>
requires (!std::is_rvalue_reference_v<ValueT>)
struct TypeVisitorResult {
    static constexpr bool value_is_ref = std::is_lvalue_reference_v<ValueT>;

    static_assert(
        value_is_ref || !std::is_const_v<ValueT>,
        "Non-reference members shouldn't be const"
    );

    using ConstTypeT = const TypeT;
    constexpr TypeVisitorResult (ConstTypeT* next_type, ValueT value) requires(value_is_ref) : next_type(next_type), value(value) {
        static_warn("Using reference type member");
    }
    constexpr TypeVisitorResult (ConstTypeT* next_type, const ValueT& value) requires(!value_is_ref) : next_type(next_type), value(value) {}
    constexpr TypeVisitorResult (ConstTypeT* next_type, ValueT&&      value) requires(!value_is_ref) : next_type(next_type), value(std::move(value)) {}

    constexpr TypeVisitorResult (const TypeVisitorResult&) = default;
    constexpr TypeVisitorResult (TypeVisitorResult&&) = default;
    
    ConstTypeT* next_type;
    ValueT value;
};

template <typename T>
struct TypeVisitorArg {
    using type = T&&;
};

template <>
struct TypeVisitorArg<void> {
    using type = estd::empty;
};

template <typename TypeT, typename ValueT = void, typename ArgT = estd::empty>
struct TypeVisitorBase {
    private:
    const Type* type;

    public:
    static constexpr bool no_value = std::is_same_v<ValueT, void>;
    static constexpr bool no_arg = std::is_same_v<ArgT, estd::empty>;
    using ConstTypeT = const TypeT;
    using ResultT = TypeVisitorResult<TypeT, ValueT>;

    constexpr explicit TypeVisitorBase (const Type* const type) : type(type) {}

    ResultT visit () const requires(no_arg) {
        return visit(estd::empty{});
    }

    ResultT visit (ArgT arg) const {
        switch (type->type) {
            case FIELD_TYPE::BOOL: {
                if constexpr (no_value) {
                    on_bool(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_bool(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::UINT8: {
                if constexpr (no_value) {
                    on_uint8(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_uint8(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::UINT16: {
                if constexpr (no_value) {
                    on_uint16(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_uint16(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::UINT32: {
                if constexpr (no_value) {
                    on_uint32(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_uint32(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::UINT64: {
                if constexpr (no_value) {
                    on_uint64(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_uint64(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::INT8: {
                if constexpr (no_value) {
                    on_int8(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_int8(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::INT16: {
                if constexpr (no_value) {
                    on_int16(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_int16(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::INT32: {
                if constexpr (no_value) {
                    on_int32(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_int32(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::INT64: {
                if constexpr (no_value) {
                    on_int64(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_int64(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::FLOAT32: {
                if constexpr (no_value) {
                    on_float32(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_float32(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::FLOAT64: {
                if constexpr (no_value) {
                    on_float64(std::forward<ArgT>(arg));
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(type + 1), on_float64(std::forward<ArgT>(arg))};
                }
            }
            case FIELD_TYPE::STRING_FIXED: {
                const FixedStringType* const fixed_string_type = type->as_fixed_string();
                if constexpr (no_value) {
                    on_fixed_string(estd::empty{}, fixed_string_type);
                    return ResultT{reinterpret_cast<ConstTypeT*>(fixed_string_type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(fixed_string_type + 1), on_fixed_string(std::forward<ArgT>(arg), fixed_string_type)};
                }
            }
            case FIELD_TYPE::STRING: {
                const StringType* const string_type = type->as_string();
                if constexpr (no_value) {
                    on_string(estd::empty{}, string_type);
                    return ResultT{reinterpret_cast<ConstTypeT*>(string_type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(string_type + 1), on_string(std::forward<ArgT>(arg), string_type)};
                }
            }
            case FIELD_TYPE::ARRAY_FIXED: {
                return on_fixed_array(std::forward<ArgT>(arg), type->as_array());
            }
            case FIELD_TYPE::ARRAY: {
                return on_array(std::forward<ArgT>(arg), type->as_array());
            }
            case FIELD_TYPE::FIXED_VARIANT: {
                return on_fixed_variant(std::forward<ArgT>(arg), type->as_fixed_variant());
            }
            case FIELD_TYPE::PACKED_VARIANT: {
                return on_packed_variant(std::forward<ArgT>(arg), type->as_packed_variant());
            }
            case FIELD_TYPE::DYNAMIC_VARIANT: {
                return on_dynamic_variant(std::forward<ArgT>(arg), type->as_dynamic_variant());
            }
            case FIELD_TYPE::IDENTIFIER: {
                const IdentifiedType* const identifier_type = type->as_identifier();
                if constexpr (no_value) {
                    on_identifier(estd::empty{},identifier_type);
                    return ResultT{reinterpret_cast<ConstTypeT*>(identifier_type + 1)};
                } else {
                    return ResultT{reinterpret_cast<ConstTypeT*>(identifier_type + 1), on_identifier(std::forward<ArgT>(arg), identifier_type)};
                }
            }
        }
    }

    virtual ValueT on_bool    (ArgT) const = 0;
    virtual ValueT on_uint8   (ArgT) const = 0;
    virtual ValueT on_uint16  (ArgT) const = 0;
    virtual ValueT on_uint32  (ArgT) const = 0;
    virtual ValueT on_uint64  (ArgT) const = 0;
    virtual ValueT on_int8    (ArgT) const = 0;
    virtual ValueT on_int16   (ArgT) const = 0;
    virtual ValueT on_int32   (ArgT) const = 0;
    virtual ValueT on_int64   (ArgT) const = 0;
    virtual ValueT on_float32 (ArgT) const = 0;
    virtual ValueT on_float64 (ArgT) const = 0;

    virtual ValueT  on_fixed_string    (ArgT, const FixedStringType*    ) const = 0;
    virtual ValueT  on_string          (ArgT, const StringType*         ) const = 0;
    virtual ResultT on_fixed_array     (ArgT, const ArrayType*          ) const = 0;
    virtual ResultT on_array           (ArgT, const ArrayType*          ) const = 0;
    virtual ResultT on_fixed_variant   (ArgT,       FixedVariantType*   ) const = 0;
    virtual ResultT on_packed_variant  (ArgT, const PackedVariantType*  ) const = 0;
    virtual ResultT on_dynamic_variant (ArgT, const DynamicVariantType* ) const = 0;
    virtual ValueT  on_identifier      (ArgT, const IdentifiedType*     ) const = 0;
};

}
