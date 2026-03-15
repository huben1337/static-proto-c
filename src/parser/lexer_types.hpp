#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <gsl/util>
#include <limits>
#include <type_traits>
#include <string_view>
#include <utility>
#include <boost/unordered/unordered_flat_map.hpp>

#include "../base.hpp"
#include "../container/memory.hpp"
#include "./memory_helpers.hpp"
#include "../estd/enum.hpp"
#include "../estd/utility.hpp"
#include "../estd/functional.hpp"
#include "../util/logger.hpp"

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

struct SIZE : estd::ENUM_CLASS<uint8_t, SIZE> {
    friend struct size_helper;

    using ENUM_CLASS::ENUM_CLASS;

    template <value_t v>
    struct Mapped;
    
    static const SIZE SIZE_1;
    static const SIZE SIZE_2;
    static const SIZE SIZE_4;
    static const SIZE SIZE_8;
    static const SIZE SIZE_0;

    static const SIZE MIN;
    static const SIZE MAX;

    [[nodiscard]] constexpr value_t byte_size () const {
        return value_t{1} << value;
    }

    template <typename Result, auto... target_sizes, typename... U, typename Visitor>
    [[nodiscard]] constexpr Result visit (this const SIZE& self, estd::variadic_v<target_sizes...> /*unused*/, Visitor&& visitor, U&&... args);

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

    template <std::unsigned_integral T>
    [[nodiscard]] static SIZE from_integral (const T i) {
        return SIZE{gsl::narrow_cast<value_t>(i % gsl::narrow_cast<value_t>(MAX.value + 1))};
    }

    template <typename writer_params>
    void log (const logger::writer<writer_params> w) const {
        w.template write<true, true>(outside_name + "_"_sl, byte_size());
    }

    struct enums;
};



constexpr SIZE SIZE::SIZE_1{0};
constexpr SIZE SIZE::SIZE_2{1};
constexpr SIZE SIZE::SIZE_4{2};
constexpr SIZE SIZE::SIZE_8{3};
constexpr SIZE SIZE::SIZE_0{static_cast<value_t>(-1)};
constexpr SIZE SIZE::MIN   {SIZE::SIZE_1};
constexpr SIZE SIZE::MAX   {SIZE::SIZE_8};

struct SIZE::enums : estd::variadic_v<SIZE_1, SIZE_2, SIZE_4, SIZE_8> {};

template <SIZE::value_t value_>
struct SIZE::Mapped {
    static constexpr SIZE value {value_};

private:
    template <auto... w>
    using includes_value = std::bool_constant<((w == value) || ...)>;

    static_assert(
        value == SIZE::SIZE_0 ||
        SIZE::enums::apply<includes_value>::value
    );
};

// namespace _size_detail {
//     template <typename Visitor, SIZE size>
//     concept size_visitor_applicable = requires (Visitor&& visitor) {
//         { visitor.template operator()<size>() } -> estd::conceptify<estd::is_any_t>;
//     };
// }

template <typename Result, auto... target_sizes, typename... U, typename Visitor>
[[nodiscard]] constexpr Result SIZE::visit (this const SIZE& self, estd::variadic_v<target_sizes...> /*unused*/, Visitor&& visitor, U&&... args) {
    switch (self) {
        case SIZE_1: if constexpr (((target_sizes == SIZE_1) || ...)) { return std::forward<Visitor>(visitor).template operator()<SIZE_1>(std::forward<U>(args)...); } else { std::unreachable(); };
        case SIZE_2: if constexpr (((target_sizes == SIZE_2) || ...)) { return std::forward<Visitor>(visitor).template operator()<SIZE_2>(std::forward<U>(args)...); } else { std::unreachable(); };
        case SIZE_4: if constexpr (((target_sizes == SIZE_4) || ...)) { return std::forward<Visitor>(visitor).template operator()<SIZE_4>(std::forward<U>(args)...); } else { std::unreachable(); };
        case SIZE_8: if constexpr (((target_sizes == SIZE_8) || ...)) { return std::forward<Visitor>(visitor).template operator()<SIZE_8>(std::forward<U>(args)...); } else { std::unreachable(); };
        case SIZE_0: if constexpr (((target_sizes == SIZE_0) || ...)) { return std::forward<Visitor>(visitor).template operator()<SIZE_0>(std::forward<U>(args)...); } else { std::unreachable(); };
        default: std::unreachable();
    }
}

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

template <SIZE size>
requires (size != SIZE::SIZE_0 && size != SIZE::SIZE_1)
constexpr SIZE next_smaller_size = size_helper::next_smaller_size<size>;

template <SIZE size>
requires (size != SIZE::SIZE_0 && size != SIZE::SIZE_8)
constexpr SIZE next_bigger_size = size_helper::next_bigger_size<size>;

template<typename T, SIZE max_alignment = SIZE::SIZE_8, SIZE min_alignment = SIZE::SIZE_1, typename Outside = void>
requires (max_alignment != SIZE::SIZE_0 && min_alignment != SIZE::SIZE_0)
struct AlignMembersBase {
    static_assert(max_alignment >= min_alignment, "max_alignment can not be smaller than min_alignment");
    
    using alignments = estd::make_index_range<min_alignment, max_alignment + 1>::template map<SIZE::Mapped>;

private:
    template <typename Outside_>
    struct outside {
        using type = Outside;
    };

    template <>
    struct outside<void> {
        using type = AlignMembersBase;
    };

    template <typename Outside_>
    static constexpr StringLiteral outside_name_v = string_literal::from_([](){ return nameof::nameof_type<Outside>(); });

    template <>
    constexpr StringLiteral outside_name_v<void> = "AlignMembersBase<>";

protected:
    using outside_t = outside<Outside>::type;
    static constexpr StringLiteral outside_name = outside_name_v<Outside>;
    static constexpr size_t alignments_count = alignments::size;

private:
    T align[alignments_count];

public:
    constexpr AlignMembersBase() = default;

    constexpr explicit AlignMembersBase (const T (&align)[alignments_count])
        : align(align) {}

    /**
     * @param aligns Values for alignments in ascending order.
     */
    template <typename... U>
    requires (alignments_count > 1 && sizeof...(U) == alignments_count)
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr AlignMembersBase (U&&... aligns)
        : align(std::forward<U>(aligns) ...) {}

    template <typename U>
    requires (alignments_count == 1)
    constexpr explicit AlignMembersBase (U&& align)
        : align(std::forward<U>(align)) {}

    #define IDENTITY(...) __VA_ARGS__

    #define ALIGN_MEMBER_GET_CT_ARG(CONST_ATTR)                                     \
    template <SIZE alignment>                                                       \
    [[nodiscard]] constexpr CONST_ATTR IDENTITY(T&) get () IDENTITY(CONST_ATTR) {   \
        static_assert(alignment <= max_alignment && alignment >= min_alignment);    \
        return align[alignment - min_alignment];                                    \
    }

    #define ALIGN_MEMBER_GET_RT_ARG(CONST_ATTR)                                                 \
    template <estd::discouraged_annotation>                                                     \
    [[nodiscard]] constexpr CONST_ATTR IDENTITY(T&) get (SIZE alignment) IDENTITY(CONST_ATTR) { \
        BSSERT(alignment <= max_alignment && alignment >= min_alignment);                       \
        return align[alignment - min_alignment];                                                \
    }

    ALIGN_MEMBER_GET_CT_ARG()
    ALIGN_MEMBER_GET_CT_ARG(const)
    ALIGN_MEMBER_GET_RT_ARG()
    ALIGN_MEMBER_GET_RT_ARG(const)

    #undef ALIGN_MEMBER_GET_CT_ARG
    #undef ALIGN_MEMBER_GET_RT_ARG
    #undef IDENDITY

private:
    template <typename writer_params, SIZE first, SIZE... rest>
    void log_ (const logger::writer<writer_params> w, estd::variadic_v<first, rest...> /*unused*/) const {
        if constexpr (sizeof...(rest) > 0) {
            w.template write<true, false>(outside_name + "{align"_sl + string_literal::from<first.byte_size()> + ": "_sl, get<first>());
            (w.template write<false, false>(", align"_sl + string_literal::from<rest.byte_size()> + ": "_sl , get<rest>()), ...);
            w.template write<false, true>("}");
        } else {
            w.template write<true, true>(outside_name + "{align"_sl + string_literal::from<first.byte_size()> + ": "_sl, get<first>(), "}");
        }
    }

public:
    template <typename writer_params>
    void log (const logger::writer<writer_params> w) const {
        log_(w, alignments{});
    }

private:
    template <typename Result, typename U, SIZE... alignments>
    [[nodiscard]] static constexpr Result of_ (U&& value, estd::variadic_v<alignments...> /*unused*/) { return Result{(static_cast<void>(alignments), std::forward<U>(value))...}; }

public:
    template <typename Result = outside_t, typename U>
    [[nodiscard]] static constexpr Result of (U&& value) { return of_<Result, U>(std::forward<U>(value), alignments{}); }
};

template<std::integral T, SIZE max_alignment = SIZE::SIZE_8, SIZE min_alignment = SIZE::SIZE_1, typename Outside = void>
struct IntegralAlignMembersBase : AlignMembersBase<T, max_alignment, min_alignment, Outside> {
private:
    using Base = AlignMembersBase<T, max_alignment, min_alignment, Outside>;
    using Base::Base;

public:
    friend Outside;

private:
    constexpr IntegralAlignMembersBase () = default;

    using outside_t = Base::outside_t;

    template <SIZE... alignments_to_check>
    requires (sizeof...(alignments_to_check) > 0)
    [[nodiscard]] constexpr SIZE _largest_align (estd::variadic_v<alignments_to_check...> /*unused*/) const {
        SIZE result;
        const bool checked_matched = (((Base::template get<alignments_to_check>() != 0) ? (result = alignments_to_check, true) : false) || ...);
        if (checked_matched) return result;
        return SIZE::SIZE_1;
    }

public:
    template <SIZE limit = max_alignment>
    requires (limit >= min_alignment && limit <= max_alignment)
    [[nodiscard]] constexpr SIZE largest_align () const {
        if constexpr (min_alignment == max_alignment) {
            return min_alignment;
        } else {
            return _largest_align(typename estd::make_index_range<min_alignment + 1, max_alignment + 1>
                    ::template apply<estd::reverse_variadic_v_t>
                    ::template map<SIZE::Mapped>{});
        }
    }

private:
    template <typename Operator, SIZE... alignments>
    [[nodiscard]] constexpr outside_t _do_binary_operation (const outside_t& other, estd::variadic_v<alignments...> /*unused*/) const {
        return outside_t{
            static_cast<T>(Operator::apply(Base::template get<alignments>(), other.template get<alignments>())) ...
        };
    }

    template <typename Operator, std::integral U, SIZE... alignments>
    [[nodiscard]] constexpr outside_t _do_binary_operation (const U& other, estd::variadic_v<alignments...> /*unused*/) const {
        return outside_t{
            static_cast<T>(Operator::apply(Base::template get<alignments>(), other)) ...
        };
    }

    template <typename Operator, typename U, SIZE... alignments>
    [[nodiscard]] constexpr outside_t do_binary_operation (const U& other) const {
        return _do_binary_operation<Operator>(other, typename Base::alignments{});
    }

    template <typename Operator, SIZE... alignments>
    [[nodiscard]] constexpr outside_t& _do_compound_binary_operation (this outside_t& self, const outside_t& other, estd::variadic_v<alignments...> /*unused*/) {
        (estd::compound_asign<Operator>(self.template get<alignments>(), other.template get<alignments>()), ...);
        return self;
    }

    template <typename Operator, std::integral U, SIZE... alignments>
    [[nodiscard]] constexpr outside_t& _do_compound_binary_operation (this outside_t& self, const U& other, estd::variadic_v<alignments...> /*unused*/) {
        (estd::compound_asign<Operator>(self.template get<alignments>(), other), ...);
        return self;
    }

    template <typename Operator, typename U, SIZE... alignments>
    [[nodiscard]] constexpr outside_t& do_compound_binary_operation (this outside_t& self, const U& other) {
        return self.template _do_compound_binary_operation<Operator>(other, typename Base::alignments{});
    }

public:
    template <std::integral U, SIZE... alignments>
    [[nodiscard]] constexpr U sum (estd::variadic_v<alignments...> /*unused*/) const {
        static_assert(
            std::numeric_limits<T>::min() >= std::numeric_limits<U>::min() &&
            std::numeric_limits<T>::max() <= std::numeric_limits<U>::max()
        );
        return (... + static_cast<U>(Base::template get<alignments>()));
    }

    template <typename Result = outside_t>
    [[nodiscard]] static consteval Result zero () { return Base::template of<Result, T>(0); }
};

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

template <std::integral T, estd::discouraged_annotation>
[[nodiscard]] constexpr T last_multiple (T value, std::type_identity_t<T> base) {
    static_warn("base must be a power of 2");
    return value & ~(base - 1);
}


template <std::integral T>
[[nodiscard]] constexpr T next_multiple (T value, SIZE base) {
    T mask = static_cast<T>(base.byte_size()) - 1;
    return (value + mask) & ~mask;
}

template <std::integral T, estd::discouraged_annotation>
[[nodiscard]] constexpr T next_multiple (T value, std::type_identity_t<T> base) {
    static_warn("base must be a power of 2");
    T mask = base - 1;
    return (value + mask) & ~mask;
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
    struct Counts : IntegralAlignMembersBase<uint16_t, SIZE::SIZE_8, SIZE::SIZE_1, Counts> {
        using IntegralAlignMembersBase::IntegralAlignMembersBase;

        [[nodiscard]] constexpr uint16_t total () const {
            return sum<uint16_t>(alignments{});
        }
    };

private:
    uint64_t data;

public:
    constexpr LeafCounts () = default;
    constexpr explicit LeafCounts (Counts counts) : data(std::bit_cast<uint64_t>(counts)) {}
    constexpr LeafCounts (uint16_t align1, uint16_t align2, uint16_t align4, uint16_t align8) : LeafCounts{{align1, align2, align4, align8}} {}
    constexpr explicit LeafCounts (uint64_t data) : data(data) {}
    constexpr explicit LeafCounts (SIZE size) : data(uint64_t{1} << (size.value * sizeof(uint16_t) * 8)) {}

    constexpr void operator += (const LeafCounts& other) { data += other.data; }

    [[nodiscard]] constexpr LeafCounts operator + (const LeafCounts& other) const { return LeafCounts{data + other.data}; }

    [[nodiscard]] constexpr bool empty () const { return data == 0; }

    [[nodiscard]] constexpr Counts counts () const { return std::bit_cast<Counts>(data); }

    [[nodiscard]] constexpr uint16_t total () const { return counts().total(); }

    [[nodiscard]] static consteval LeafCounts zero () { return LeafCounts{Counts::zero()}; }
    [[nodiscard]] static constexpr LeafCounts of (uint16_t value) { return LeafCounts{Counts::of(value)}; }

    template <SIZE size>
    requires (size != SIZE::SIZE_0)
    [[nodiscard]] static consteval LeafCounts from_size () {
        return LeafCounts{size};
    }

    template <SIZE limit = lexer::SIZE::SIZE_8>
    [[nodiscard]] constexpr SIZE largest_align () const {
        return counts().largest_align<limit>();
    }
};

struct LeafSizes : IntegralAlignMembersBase<uint64_t, SIZE::SIZE_8, SIZE::SIZE_1, LeafSizes> {
    using IntegralAlignMembersBase::IntegralAlignMembersBase;

private:
    template <SIZE... alignments>
    constexpr LeafSizes (const LeafCounts::Counts& counts, estd::variadic_v<alignments...> /*unused*/)
        : IntegralAlignMembersBase{counts.get<alignments>()...} {}

public:
    constexpr explicit LeafSizes (const LeafCounts::Counts& counts)
        : LeafSizes{counts, LeafCounts::Counts::alignments{}} {}

    constexpr LeafSizes& operator += (const LeafSizes& other) {
        return do_compound_binary_operation<estd::add>(other);
    }

    [[nodiscard]] constexpr LeafSizes operator + (const LeafSizes& other) const {
        return do_binary_operation<estd::add>(other);
    }

    [[nodiscard]] constexpr LeafSizes operator * (const uint32_t factor) const {
        return do_binary_operation<estd::multiply>(factor);
    }

    [[nodiscard]] constexpr uint64_t total () const {
        return sum<uint64_t>(alignments{});
    }

    [[nodiscard]] constexpr bool empty () const {
        return total() == 0;
    }


    template <estd::discouraged_annotation>
    [[nodiscard]] static constexpr LeafSizes from_space_at_size (const SIZE size, const uint64_t space) {
        LeafSizes result = LeafSizes::zero();
        result.get<estd::discouraged>(size) = space;
        return result;
    }

    template <estd::discouraged_annotation>
    [[nodiscard]] static constexpr LeafSizes from_size (const SIZE size, const uint32_t count) {
        return from_space_at_size<estd::discouraged>(size, static_cast<uint64_t>(size.byte_size()) * count);
    }

    template <SIZE size>
    [[nodiscard]] static constexpr LeafSizes from_space_at_size (const uint64_t space) {
        LeafSizes result = LeafSizes::zero();
        result.get<size>() = space;
        return result;
    }
    
    template <SIZE size>
    [[nodiscard]] static constexpr LeafSizes from_size (const uint32_t count) {
        return from_space_at_size<size>(static_cast<uint64_t>(size.byte_size()) * count);
    }

    template <SIZE size>
    [[nodiscard]] static consteval LeafSizes from_size () {
        return from_size<size>(1);
    }

    
};

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
    [[nodiscard]] auto* as_array () const;
    [[nodiscard]] auto* as_fixed_variant () const;
    [[nodiscard]] auto* as_packed_variant () const;
    [[nodiscard]] auto* as_dynamic_variant () const;

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

        constexpr VisitResult (cont_next_type_t* next_type, ValueT value) requires(value_is_ref)
            : next_type(next_type), value(value) {
            static_warn("Using reference type member");
        }
        constexpr VisitResult (cont_next_type_t* next_type, const ValueT& value) requires(!value_is_ref)
            : next_type(next_type), value(value) {}
        constexpr VisitResult (cont_next_type_t* next_type, ValueT&&      value) requires(!value_is_ref)
            : next_type(next_type), value(std::move(value)) {}
        
        cont_next_type_t* next_type;
        ValueT value;
    };

    template <typename NextTypeT>
    struct VisitResult<NextTypeT, void> {
        using cont_next_type_t = const NextTypeT;
        using value_t = void;
        constexpr explicit VisitResult (cont_next_type_t*&& next_type)
            : next_type(std::move(next_type)) {}
        
        cont_next_type_t* next_type;
    };

    template <typename VisitorT, typename... ArgsT>
    [[nodiscard]] std::remove_cvref_t<VisitorT>::result_t visit (VisitorT&& visitor, ArgsT&&... args) const;

    template <typename T>
    [[nodiscard]] inline T* skip () const;
};


template <typename T>
[[nodiscard, gnu::always_inline]] inline T* get_extended_type(auto* that) {
    return get_extended<T, Type>(that);
}


struct FixedStringType {
    static void create (Buffer &buffer, uint32_t length) {
        auto [extended, base] = create_extended<FixedStringType, Type>(buffer);
        *base = {FIELD_TYPE::STRING_FIXED};
        *extended = {
            length,
            get_size_size(length)
        };
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
        *base = {STRING};
        *extended = {
            min_length,
            stored_size_size,
            size_size
        };
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
        *base = {IDENTIFIER};
        *extended = {identifier_idx};
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

    LeafCounts level_fixed_leafs;
    uint32_t length;
    uint16_t pack_info_base_idx;
    SIZE stored_size_size;
    SIZE size_size;

    [[nodiscard]] const Type* inner_type () const {
        return estd::ptr_cast<const Type>(this + 1);
    }

};
[[nodiscard]] inline auto* Type::as_array () const {
    return get_extended_type<ArrayType>(this);
}



template <typename TypeMetaT>
struct VariantTypeBase {
    friend struct Type;
    
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

    [[nodiscard]] const Type* first_variant() const {
        return estd::ptr_cast<const Type>(this + 1);
    }
private:
    template <typename T>
    [[nodiscard]] T* after () const {
        return estd::ptr_cast<T>(type_metas() + variant_count);
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
            return estd::ptr_cast<const Type>(this + 1);
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

    EnumField* next () {
        return this + 1;
    }
};

struct StructDefinition : IdentifiedDefinition::Data {
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

    [[nodiscard]] static auto create(Buffer &buffer) {
        return __create_extended<StructDefinition, IdentifiedDefinition>(buffer);
    }

    static StructField::Data* reserve_field(Buffer &buffer) {
        // Since we can create a Data object any time the current buffer position might not be aligned, so we have to ensure alignment.
        return create_padded<StructField::Data>(buffer);
    }
private:
    [[nodiscard]] const StructField* first_field() const {
        return estd::ptr_cast<const StructField>(this + 1);
    }
public:
    template <typename VisitorT>
    void visit (VisitorT&& visitor) const {
        const StructField* field = first_field();
        for (uint16_t i = 0; i < field_count; i++) {
            field = visitor(field->data());
        }
    }

    template <typename VisitorT>
    void visit_uninitialized (VisitorT&& visitor, const uint16_t field_count) const {
        const StructField* field = first_field();
        for (uint16_t i = 0; i < field_count; i++) {
            field = visitor(field->data());
        }
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
        return estd::ptr_cast<EnumField>(this + 1);
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
[[nodiscard]] inline T* Type::skip () const {
    switch (type) {
        case STRING_FIXED:      return estd::ptr_cast<T>(as_fixed_string() + 1);
        case STRING:            return estd::ptr_cast<T>(as_string() + 1);
        case ARRAY_FIXED:
        case ARRAY:             return as_array()->inner_type()->skip<T>();
        case FIXED_VARIANT:     return as_fixed_variant()->after<T>();
        case PACKED_VARIANT:    return as_packed_variant()->after<T>();
        case DYNAMIC_VARIANT:   return as_dynamic_variant()->after<T>();
        case IDENTIFIER:        return estd::ptr_cast<T>(as_identifier() + 1);
        default:                return estd::ptr_cast<T>(this + 1);
    }
}

template <typename T, typename TypeMeta_T>
inline const T* skip_variant_type (const VariantTypeBase<TypeMeta_T>* variant_type) {
    return variant_type->template after<const T>();
}

template <typename VisitorT, typename... ArgsT>
[[nodiscard]] inline std::remove_cvref_t<VisitorT>::result_t Type::visit (VisitorT&& visitor, ArgsT&&... args) const {
    using visitor_t = std::remove_cvref_t<VisitorT>;
    using result_t = visitor_t::result_t;
    using const_next_type_t = const visitor_t::next_type_t;
    static constexpr bool no_value = std::is_same_v<typename result_t::value_t, void>;

    switch (this->type) {
        case FIELD_TYPE::BOOL: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_bool(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_bool(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::UINT8: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_uint8(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_uint8(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::UINT16: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_uint16(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_uint16(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::UINT32: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_uint32(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_uint32(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::UINT64: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_uint64(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_uint64(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::INT8: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_int8(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_int8(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::INT16: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_int16(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_int16(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::INT32: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_int32(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_int32(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::INT64: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_int64(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_int64(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::FLOAT32: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_float32(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_float32(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::FLOAT64: {
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_float64(std::forward<ArgsT>(args)...);
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(this + 1),
                    std::forward<VisitorT>(visitor).on_float64(std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::STRING_FIXED: {
            const FixedStringType* const fixed_string_type = this->as_fixed_string();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_fixed_string(fixed_string_type);
                return result_t{estd::ptr_cast<const_next_type_t>(fixed_string_type + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(fixed_string_type + 1),
                    std::forward<VisitorT>(visitor).on_fixed_string(fixed_string_type, std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::STRING: {
            const StringType* const string_type = this->as_string();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_string(string_type);
                return result_t{estd::ptr_cast<const_next_type_t>(string_type + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(string_type + 1),
                    std::forward<VisitorT>(visitor).on_string(string_type, std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::ARRAY_FIXED: {
            return std::forward<VisitorT>(visitor).on_fixed_array(this->as_array(), std::forward<ArgsT>(args)...);
        }
        case FIELD_TYPE::ARRAY: {
            return std::forward<VisitorT>(visitor).on_array(this->as_array(), std::forward<ArgsT>(args)...);
        }
        case FIELD_TYPE::FIXED_VARIANT: {
            FixedVariantType* const fixed_variant_type = this->as_fixed_variant();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_fixed_variant(fixed_variant_type, std::forward<ArgsT>(args)...);
                return result_t{fixed_variant_type->after<const_next_type_t>()};
            } else {
                return result_t{fixed_variant_type->after<const_next_type_t>(),
                    std::forward<VisitorT>(visitor).on_fixed_variant(fixed_variant_type, std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::PACKED_VARIANT: {
            PackedVariantType* const packed_variant_type = this->as_packed_variant();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_packed_variant(packed_variant_type, std::forward<ArgsT>(args)...);
                return result_t{packed_variant_type->after<const_next_type_t>()};
            } else {
                return result_t{packed_variant_type->after<const_next_type_t>(),
                    std::forward<VisitorT>(visitor).on_packed_variant(packed_variant_type, std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::DYNAMIC_VARIANT: {
            DynamicVariantType* const dynamic_variant_type = this->as_dynamic_variant();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_dynamic_variant(dynamic_variant_type, std::forward<ArgsT>(args)...);
                return result_t{dynamic_variant_type->after<const_next_type_t>()};
            } else {
                return result_t{dynamic_variant_type->after<const_next_type_t>(),
                    std::forward<VisitorT>(visitor).on_dynamic_variant(dynamic_variant_type, std::forward<ArgsT>(args)...)};
            }
        }
        case FIELD_TYPE::IDENTIFIER: {
            const IdentifiedType* const identifier_type = this->as_identifier();
            if constexpr (no_value) {
                std::forward<VisitorT>(visitor).on_identifier(identifier_type);
                return result_t{estd::ptr_cast<const_next_type_t>(identifier_type + 1)};
            } else {
                return result_t{estd::ptr_cast<const_next_type_t>(identifier_type + 1),
                    std::forward<VisitorT>(visitor).on_identifier(identifier_type, std::forward<ArgsT>(args)...)};
            }
        }
        default: {
            std::unreachable();
        }
    }
}

}
