#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <gsl/util>
#include <limits>
#include <string_view>
#include <type_traits>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <utility>

#include "../helper/internal_error.hpp"
#include "./lexer_types.hpp"
#include "./lex_error.hpp"
#include "./lex_helpers.re2c.hpp"
#include "./parse_int.re2c.hpp"
#include "../estd/empty.hpp"
#include "../util/string_view_equal.hpp"
#include "../util/logger.hpp"

namespace lexer {

template <typename T>
struct LexResult {
    const char *cursor;
    T value;
};

/*!re2c
    re2c:yyfill:enable = 0;
    re2c:define:YYCTYPE = char;

    re2c:api = generic;
    re2c:api:style = free-form;

    re2c:define:YYBACKUP     = "// YYBACKUP";
    re2c:define:YYBACKUPCTX  = "YYBACKUPCTX";
    re2c:define:YYLESSTHAN   = "YYLESSTHAN";
    re2c:define:YYMTAGN      = "YYMTAGN";
    re2c:define:YYMTAGP      = "YYMTAGP";
    re2c:define:YYPEEK       = "*YYCURSOR";
    re2c:define:YYRESTORE    = "// YYRESTORE";
    re2c:define:YYRESTORECTX = "YYRESTORECTX";
    re2c:define:YYRESTORETAG = "YYRESTORETAG";
    re2c:define:YYSKIP       = "++YYCURSOR;";
    re2c:define:YYSHIFT      = "YYSHIFT";
    re2c:define:YYCOPYMTAG   = "YYCOPYMTAG";
    re2c:define:YYCOPYSTAG   = "@@{lhs} = @@{rhs};";
    re2c:define:YYSHIFTMTAG  = "YYSHIFTMTAG";
    re2c:define:YYSHIFTSTAG  = "@@{tag} += @@{shift};// YYSHIFTSTAG";
    re2c:define:YYSTAGN      = "YYSTAGN";
    re2c:define:YYSTAGP      = "@@{tag} = YYCURSOR;";

    any_white_space = [ \t\r\n];
    white_space = [ \t];
*/

inline const char* lex_argument_list_start (const char* YYCURSOR) {
    return lex_symbol<'<', "expected argument list">(YYCURSOR);
}


inline const char* lex_argument_list_end (const char* YYCURSOR) {
    return lex_symbol<'>', "expected end of argument list">(YYCURSOR);
}

template <
    typename T,
    bool fail_on_fixed = false,
    bool fail_on_range = false,
    typename F,
    typename G,
    typename... ArgsT
>
inline T lex_range_argument (const char* YYCURSOR, F on_fixed, G on_range, ArgsT&&... args) {

    auto parsed_0 = parse_uint_skip_white_space<uint32_t, true>(YYCURSOR);
    YYCURSOR = parsed_0.cursor;
    const char* const range_start = YYCURSOR - parsed_0.digits;
    auto min = parsed_0.value;

    const char* const cursor_after_min = YYCURSOR;

    /*!local:re2c
        any_white_space* "." { goto maybe_range; }
        any_white_space* { goto fixed; }
    */

    maybe_range:
    if (*YYCURSOR == '.') {
        auto parsed_1 = parse_uint_skip_white_space<uint32_t>(YYCURSOR + 1);
        YYCURSOR = parsed_1.cursor;
        auto max = parsed_1.value;
        if (max <= min) [[unlikely]] {
            show_syntax_error("invalid range", range_start, YYCURSOR - 1);
        }
        if constexpr (fail_on_range) {
            on_range(YYCURSOR);
            std::unreachable();
        } else {
            return on_range(YYCURSOR, min, max, std::forward<ArgsT>(args)...);
        }  
    } else {
        UNEXPECTED_INPUT("expected '..' to mark range");
    }

    fixed:
    if constexpr (fail_on_fixed) {
        on_fixed(cursor_after_min);
        std::unreachable();
    } else {
        return on_fixed(cursor_after_min, min, std::forward<ArgsT>(args)...);
    }
}

template <std::unsigned_integral T, T max = std::numeric_limits<T>::max()>
inline ParseNumberResult<T> lex_attribute_value (const char* YYCURSOR) {
    YYCURSOR = lex_symbol<'=', "Expected value assignment for attribute">(YYCURSOR);
    return parse_uint_skip_white_space<T, false, max>(YYCURSOR);
}

struct VariantAttributes {
    using shared_id_t = uint32_t;
    static constexpr shared_id_t NO_SHARED_ID = -1;
    uint64_t max_wasted_bytes = 32;
    shared_id_t shared_id = NO_SHARED_ID;

    [[nodiscard]] constexpr bool has_shared_id () const { return shared_id != NO_SHARED_ID; };
};

inline LexResult<VariantAttributes> lex_variant_attributes (const char* YYCURSOR) {

    /*!local:re2c
        white_space* "["    { goto maybe_lex_attributes; }
        white_space*        { goto no_attributes; }
    */

    no_attributes: {
        return {YYCURSOR, {}};
    }

    maybe_lex_attributes: {
        VariantAttributes attributes;
        bool has_max_wasted_bytes = false;

        /*!local:re2c
            "[" { goto lex_attributes; }
            * { show_syntax_error("expected second '[' to open attribute list", YYCURSOR - 1); }
        */

        lex_attributes: {
            /*!local:re2c
                white_space* "max_wasted"   { goto lex_max_wasted_bytes; }
                white_space* "shared_id"    { goto lex_shared_id; }
                white_space*                { show_syntax_error("expected attribute", YYCURSOR - 1); }
            */
            lex_max_wasted_bytes: {
                if (has_max_wasted_bytes) {
                    show_syntax_error("conflicting attributes", YYCURSOR - 1);
                }
                has_max_wasted_bytes = true;
                auto parsed = lex_attribute_value<uint32_t>(YYCURSOR);
                attributes.max_wasted_bytes = parsed.value;
                YYCURSOR = parsed.cursor;
                goto attribute_end;
            }
            lex_shared_id: {
                if (attributes.has_shared_id()) {
                    show_syntax_error("conflicting attributes", YYCURSOR - 1);
                }
                auto parsed = lex_attribute_value<uint32_t, VariantAttributes::NO_SHARED_ID - 1>(YYCURSOR);
                attributes.shared_id = parsed.value;
                YYCURSOR = parsed.cursor;
                goto attribute_end;
            }
            attribute_end: {
                /*!local:re2c
                    white_space* "]" { goto close_attributes; }
                    white_space* "," { goto lex_attributes; }
                    white_space* { show_syntax_error("expected end of attributes or next ", YYCURSOR); }
                */
            }
        }

        close_attributes:
        /*!local:re2c
            "]" { goto done; }
            * { show_syntax_error("expected closing of attributes", YYCURSOR); }
        */
        done:
        return {YYCURSOR, attributes};
    }

}


inline LexResult<std::string_view> lex_identifier_name (const char* YYCURSOR) {
    const char* start;

    /*!stags:re2c format = 'const char *@@;\n'; */
    /*!local:re2c
        re2c:tags = 1;
        identifier = [a-zA-Z_][a-zA-Z0-9_]*;

        invalid = [^a-zA-Z0-9_];

        any_white_space* @start identifier      { goto done; }

        any_white_space* "int8"    invalid      { UNEXPECTED_INPUT("int8 is reserved");    }
        any_white_space* "int16"   invalid      { UNEXPECTED_INPUT("int16 is reserved");   }
        any_white_space* "int32"   invalid      { UNEXPECTED_INPUT("int32 is reserved");   }
        any_white_space* "int64"   invalid      { UNEXPECTED_INPUT("int64 is reserved");   }
        any_white_space* "uint8"   invalid      { UNEXPECTED_INPUT("uint8 is reserved");   }
        any_white_space* "uint16"  invalid      { UNEXPECTED_INPUT("uint16 is reserved");  }
        any_white_space* "uint32"  invalid      { UNEXPECTED_INPUT("uint32 is reserved");  }
        any_white_space* "uint64"  invalid      { UNEXPECTED_INPUT("uint64 is reserved");  }
        any_white_space* "float32" invalid      { UNEXPECTED_INPUT("float32 is reserved"); }
        any_white_space* "float64" invalid      { UNEXPECTED_INPUT("float64 is reserved"); }
        any_white_space* "bool"    invalid      { UNEXPECTED_INPUT("bool is reserved");    }
        any_white_space* "string"  invalid      { UNEXPECTED_INPUT("string is reserved");  }
        any_white_space* "array"   invalid      { UNEXPECTED_INPUT("array is reserved");   }
        any_white_space* "variant" invalid      { UNEXPECTED_INPUT("variant is reserved"); }
        any_white_space* "struct"  invalid      { UNEXPECTED_INPUT("struct is reserved");  }
        any_white_space* "enum"    invalid      { UNEXPECTED_INPUT("enum is reserved");    }
        any_white_space* "union"   invalid      { UNEXPECTED_INPUT("union is reserved");   }
        any_white_space* "target"  invalid      { UNEXPECTED_INPUT("target is reserved");  }

        any_white_space* { UNEXPECTED_INPUT("expected name"); }
    */
    done:
    return { YYCURSOR, {start, YYCURSOR} };
}

inline void add_identifier (IdentifierMap &identifier_map, std::string_view name, IdentifedDefinitionIndex definition_idx) {
    bool did_emplace = identifier_map.emplace(std::pair{name, definition_idx}).second;
    if (!did_emplace) {
        show_syntax_error("identifier already defined", name.begin(), name.end());
    }
}


struct LexTypeResult {
    const char* cursor;
    LeafCounts level_fixed_leafs;
    LeafCounts var_leaf_counts;
    uint64_t min_byte_size;
    uint64_t max_byte_size;
    uint16_t level_fixed_variants;
    uint16_t level_fixed_arrays;
    uint16_t level_variant_fields;
    uint16_t sublevel_fixed_leafs;
    uint16_t pack_count;
    uint16_t total_variant_var_leafs;
    uint16_t level_size_leafs;
    SIZE alignment;
};

struct LexFixedTypeResult {
    const char* cursor;
    LeafCounts level_fixed_leafs;
    uint64_t byte_size;
    uint16_t level_fixed_variants;
    uint16_t level_fixed_arrays;
    uint16_t level_variant_fields;
    uint16_t sublevel_fixed_leafs;
    uint16_t pack_count;
    SIZE alignment;
};

template <bool is_dynamic>
using variant_type_meta_t = std::conditional_t<is_dynamic, DynamicVariantTypeMeta, FixedVariantTypeMeta>;

template <bool expect_fixed>
std::conditional_t<expect_fixed, LexFixedTypeResult, LexTypeResult> lex_type (const char* YYCURSOR, Buffer &buffer, IdentifierMap &identifier_map);

template <bool is_dynamic, bool expect_fixed, typename BufferedTypeMeta>
inline std::conditional_t<expect_fixed, LexFixedTypeResult, LexTypeResult> add_variant_type (
    const char* YYCURSOR,
    uint64_t inner_min_byte_size,
    uint64_t inner_max_byte_size,
    Buffer& buffer,
	Buffer&& type_meta_buffer,
    const __CreateExtendedResult<DynamicVariantType, Type> created_variant_type,
    const uint16_t variant_count,
    const uint16_t sublevel_fixed_leafs,
    const uint16_t pack_count,
    const uint16_t total_variant_var_leafs,
    const SIZE max_alignment
) {
    auto attribute_lex_result = lex_variant_attributes(YYCURSOR);
    YYCURSOR = attribute_lex_result.cursor;
    auto [max_wasted_bytes, shared_id] = attribute_lex_result.value;

    using out_type_meta_t = variant_type_meta_t<is_dynamic>;

    const size_t meta_padding = get_padding<out_type_meta_t>(buffer.position_idx<uint8_t>().value);
    
    const Buffer::Index<out_type_meta_t> meta_dst_idx {buffer.next_multi_byte<uint8_t>(
        (variant_count * sizeof(out_type_meta_t)) + meta_padding
    ).add(meta_padding).value};
    
    out_type_meta_t* meta_dst = buffer.get_aligned(meta_dst_idx);
    BufferedTypeMeta* meta_src = type_meta_buffer.get(Buffer::Index<BufferedTypeMeta>{0});
    for (size_t i = 0; i < variant_count; i++) {
        if constexpr (expect_fixed) {
            static_assert(!is_dynamic);
            *meta_dst = *meta_src;
        } else if constexpr (is_dynamic) {
            *meta_dst = *meta_src;
        } else {
            *meta_dst = {
                meta_src->level_fixed_leafs,
                meta_src->level_fixed_variants,
                meta_src->level_fixed_arrays
            };
        }
        meta_dst++;
        meta_src++;
    }

    if constexpr (is_dynamic) {
        console.debug("Lexer found DYNAMIC_VARIANT");
        *buffer.get(created_variant_type.base) = {DYNAMIC_VARIANT};
    } else {
        if ((inner_max_byte_size - inner_min_byte_size) > max_wasted_bytes) {
            console.debug("Packing variant to satisfy size requirements");
            *buffer.get(created_variant_type.base) = {PACKED_VARIANT};
        } else {
            *buffer.get(created_variant_type.base) = {FIXED_VARIANT};
        }
    }

    auto* const variant_type = buffer.get_aligned(created_variant_type.extended);
    BSSERT(meta_dst_idx.value > created_variant_type.extended.value);
    const Buffer::index_t type_metas_offset = meta_dst_idx.value - created_variant_type.extended.value;

    // inner_max_byte_size = next_multiple(inner_max_byte_size, max_alignment);
    uint64_t max_byte_size = inner_max_byte_size;
    uint64_t min_byte_size = inner_min_byte_size;

    LeafCounts level_fixed_leafs;

    // Init level_fixed_leafs and add the id leaf and its metadata
    #define ADD_ID_LEAF(SIZE) \
    level_fixed_leafs = LeafCounts::from_size<SIZE>(); \
    max_byte_size += (SIZE).byte_size(); \
    min_byte_size += (SIZE).byte_size();

    if (variant_count <= UINT8_MAX) {
        ADD_ID_LEAF(SIZE::SIZE_1)
    } else {
        ADD_ID_LEAF(SIZE::SIZE_2)
    }

    #undef ADD_ID_LEAF


    if constexpr (is_dynamic) {
        uint64_t max = inner_max_byte_size;
        uint64_t delta = inner_max_byte_size - inner_min_byte_size;
        SIZE stored_size_size;
        SIZE size_size;

        #define ADD_SIZE_LEAF_PRE(SIZE) \
        level_fixed_leafs += LeafCounts::from_size<SIZE>(); \
        max_byte_size += (SIZE).byte_size(); \
        stored_size_size = SIZE;

        if (delta <= UINT8_MAX) {
            ADD_SIZE_LEAF_PRE(SIZE::SIZE_1)
            if (max <= UINT8_MAX) {
                size_size = SIZE::SIZE_1;
            } else if (max <= UINT16_MAX) {
                size_size = SIZE::SIZE_2;
            } else if (max <= UINT32_MAX) {
                size_size = SIZE::SIZE_4;
            } else {
                size_size = SIZE::SIZE_8;
            }
        } else if (delta <= UINT16_MAX) {
            ADD_SIZE_LEAF_PRE(SIZE::SIZE_2)
            if (max <= UINT16_MAX) {
                size_size = SIZE::SIZE_2;
            } else if (max <= UINT32_MAX) {
                size_size = SIZE::SIZE_4;
            } else {
                size_size = SIZE::SIZE_8;
            }
        } else if (delta <= UINT32_MAX) {
            ADD_SIZE_LEAF_PRE(SIZE::SIZE_4)
            if (max <= UINT32_MAX) {
                size_size = SIZE::SIZE_4;
            } else {
                size_size = SIZE::SIZE_8;
            }
        } else {
            ADD_SIZE_LEAF_PRE(SIZE::SIZE_8)
            size_size = SIZE::SIZE_8;
        }

        #undef ADD_SIZE_LEAF_PRE

        *variant_type = {
            inner_min_byte_size,
            type_metas_offset,
            variant_count,
            sublevel_fixed_leafs,
            total_variant_var_leafs,
            stored_size_size,
            size_size
        };
    } else {
        *variant_type = {
            static_cast<uint64_t>(-1),
            type_metas_offset,
            variant_count,
            sublevel_fixed_leafs,
            total_variant_var_leafs,
            SIZE::SIZE_0,
            SIZE::SIZE_0
        };
    }



    if constexpr (expect_fixed) {
        return LexFixedTypeResult{
            YYCURSOR,
            level_fixed_leafs,
            inner_max_byte_size,
            1,
            0,
            1,
            sublevel_fixed_leafs,
            gsl::narrow_cast<uint16_t>(pack_count + 4),
            max_alignment
        };
    } else {
        if constexpr (is_dynamic) {
            return LexTypeResult{
                YYCURSOR,
                level_fixed_leafs,
                LeafCounts{max_alignment},
                min_byte_size,
                max_byte_size,
                0,
                0,
                1,
                sublevel_fixed_leafs,
                gsl::narrow_cast<uint16_t>(pack_count + 4),
                total_variant_var_leafs,
                1,
                max_alignment
            };
        } else {
            return LexTypeResult{
                YYCURSOR,
                level_fixed_leafs,
                LeafCounts::zero(),
                min_byte_size,
                max_byte_size,
                1,
                0,
                1,
                sublevel_fixed_leafs,
                gsl::narrow_cast<uint16_t>(pack_count + 4),
                total_variant_var_leafs,
                0,
                max_alignment
            };
        }
    }
}

template <bool is_dynamic, bool expect_fixed, typename BufferedTypeMetaT>
inline std::conditional_t<expect_fixed, LexFixedTypeResult, LexTypeResult> lex_variant_types (
    const char* YYCURSOR,
    uint64_t min_byte_size,
    uint64_t max_byte_size,
    Buffer& buffer,
	Buffer&& type_meta_buffer,
    IdentifierMap& identifier_map,
    __CreateExtendedResult<DynamicVariantType, Type> created_variant_type,
    uint16_t variant_count,
    uint16_t sublevel_fixed_leafs,
    uint16_t pack_count,
    uint16_t total_variant_var_leafs,
    SIZE max_alignment
) {
    while (true) {
        variant_count++;
        auto result = lex_type<expect_fixed>(YYCURSOR, buffer, identifier_map);
        YYCURSOR = result.cursor;

        if constexpr (expect_fixed) {
            *type_meta_buffer.get_next<FixedVariantTypeMeta>() = {
                result.level_fixed_leafs,
                result.level_fixed_variants,
                result.level_fixed_arrays
            };
        } else {
            *type_meta_buffer.get_next<DynamicVariantTypeMeta>() = {
                result.level_fixed_leafs,
                result.var_leaf_counts,
                result.level_fixed_variants,
                result.level_fixed_arrays,
                result.level_size_leafs
            };
            total_variant_var_leafs += result.var_leaf_counts.total() + result.total_variant_var_leafs;
        }
        sublevel_fixed_leafs += result.level_fixed_leafs.total() + result.sublevel_fixed_leafs;
        pack_count += result.pack_count;

        max_alignment = std::max(max_alignment, result.alignment);
        if constexpr (expect_fixed) {
            min_byte_size = std::min(min_byte_size, result.byte_size);
            max_byte_size = std::max(max_byte_size, result.byte_size);
        } else {
            min_byte_size = std::min(min_byte_size, result.min_byte_size);
            max_byte_size = std::max(max_byte_size, result.max_byte_size);
        }

        if constexpr (!is_dynamic && !expect_fixed) {
            if (!result.var_leaf_counts.empty()) {
                /*!local:re2c
                    any_white_space* "," { goto dynamic_variant_next; }
                    any_white_space* ">" { goto dynamic_variant_done;  }
                    any_white_space* { UNEXPECTED_INPUT("expected ',' or '>'"); }
                */
                dynamic_variant_next: {
                    return lex_variant_types<true, expect_fixed, BufferedTypeMetaT>(
                        YYCURSOR,
                        min_byte_size,
                        max_byte_size,
                        buffer,
                        std::move(type_meta_buffer),
                        identifier_map,
                        created_variant_type,
                        variant_count,
                        sublevel_fixed_leafs,
                        pack_count,
                        total_variant_var_leafs,
                        max_alignment
                    );
                }
                dynamic_variant_done: {
                    return add_variant_type<true, expect_fixed, BufferedTypeMetaT>(
                        YYCURSOR,
                        min_byte_size,
                        max_byte_size,
                        buffer,
                        std::move(type_meta_buffer),
                        created_variant_type,
                        variant_count,
                        sublevel_fixed_leafs,
                        pack_count,
                        total_variant_var_leafs,
                        max_alignment
                    );
                }
            }
        }

        /*!local:re2c
            any_white_space* "," { continue; }
            any_white_space* ">" { goto variant_done; }
            any_white_space* { UNEXPECTED_INPUT("expected ',' or '>'"); }
        */
        variant_done: {
            return add_variant_type<is_dynamic, expect_fixed, BufferedTypeMetaT>(
                YYCURSOR,
                min_byte_size,
                max_byte_size,
                buffer,
                std::move(type_meta_buffer),
                created_variant_type,
                variant_count,
                sublevel_fixed_leafs,
                pack_count,
                total_variant_var_leafs,
                max_alignment
            );
        }
    }
}

template <bool expect_fixed, FIELD_TYPE field_type>
[[nodiscard]] inline std::conditional_t<expect_fixed, LexFixedTypeResult, LexTypeResult> add_simple_type (const char* YYCURSOR, Buffer &buffer) {
    static_assert(
           field_type == FIELD_TYPE::BOOL
        || field_type == FIELD_TYPE::INT8
        || field_type == FIELD_TYPE::UINT8
        || field_type == FIELD_TYPE::INT16
        || field_type == FIELD_TYPE::UINT16
        || field_type == FIELD_TYPE::INT32
        || field_type == FIELD_TYPE::UINT32
        || field_type == FIELD_TYPE::INT64
        || field_type == FIELD_TYPE::UINT64
        || field_type == FIELD_TYPE::FLOAT32
        || field_type == FIELD_TYPE::FLOAT64,
        "unsupported type for simple_type"
    );
    *buffer.get_next<Type>() = {field_type};
    constexpr lexer::SIZE alignment = get_type_alignment<field_type>();
    if constexpr (expect_fixed) {
        return LexFixedTypeResult{
            YYCURSOR,
            LeafCounts::from_size<alignment>(),
            0,
            0,
            0,
            0,
            0,
            0,
            alignment
        };
    } else {
        constexpr uint64_t byte_size = alignment.byte_size();
        return LexTypeResult{
            YYCURSOR,
            LeafCounts::from_size<alignment>(),
            LeafCounts::zero(),
            byte_size,
            byte_size,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            alignment
        };
    }
}

template <bool expect_fixed>
std::conditional_t<expect_fixed, LexFixedTypeResult, LexTypeResult> lex_type (const char* YYCURSOR, Buffer &buffer, IdentifierMap &identifier_map) {

    const char* typename_start; // Only initialized for non-simple types

    #define ADD_SIMPLE_TYPE(TYPE) \
    return add_simple_type<expect_fixed, FIELD_TYPE::TYPE>(YYCURSOR, buffer);


    /*!stags:re2c format = 'const char *@@;\n'; */
    /*!local:re2c
        re2c:tags = 1;
        identifier = [a-zA-Z_][a-zA-Z0-9_]*;

        any_white_space* "int8"                             { ADD_SIMPLE_TYPE(INT8   ) }
        any_white_space* "int16"                            { ADD_SIMPLE_TYPE(INT16  ) }
        any_white_space* "int32"                            { ADD_SIMPLE_TYPE(INT32  ) }
        any_white_space* "int64"                            { ADD_SIMPLE_TYPE(INT64  ) }
        any_white_space* "uint8"                            { ADD_SIMPLE_TYPE(UINT8  ) }
        any_white_space* "uint16"                           { ADD_SIMPLE_TYPE(UINT16 ) }
        any_white_space* "uint32"                           { ADD_SIMPLE_TYPE(UINT32 ) }
        any_white_space* "uint64"                           { ADD_SIMPLE_TYPE(UINT64 ) }
        any_white_space* "float32"                          { ADD_SIMPLE_TYPE(FLOAT32) }
        any_white_space* "float64"                          { ADD_SIMPLE_TYPE(FLOAT64) }
        any_white_space* "bool"                             { ADD_SIMPLE_TYPE(BOOL   ) }
        any_white_space*  @typename_start "string"          { goto string;             }
        any_white_space*  @typename_start "array"           { goto array;              }
        any_white_space*  @typename_start "variant"         { goto variant;            }
        any_white_space*  @typename_start identifier        { goto identifier;         }

        any_white_space* { UNEXPECTED_INPUT("expected type"); }
    */

    string: {
        YYCURSOR = lex_argument_list_start(YYCURSOR);

        if constexpr (!expect_fixed) {
            return lex_range_argument<LexTypeResult>(
                YYCURSOR,
                [](const char* cursor, uint32_t length, Buffer& buffer)->LexTypeResult {
                    FixedStringType::create(buffer, length);

                    return LexTypeResult{
                        lex_argument_list_end(cursor),
                        LeafCounts::from_size<SIZE::SIZE_1>(),
                        LeafCounts::zero(),
                        length,
                        length,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        SIZE::SIZE_1
                    };
                },
                [](const char* cursor, uint32_t min_length, uint32_t max_length, Buffer& buffer)->LexTypeResult {
                    uint32_t delta = min_length - max_length;

                    LeafCounts level_fixed_leafs;

                    uint64_t min_byte_size;
                    uint64_t max_byte_size;

                    SIZE size_size;
                    SIZE stored_size_size;

                    #define ADD_SIZE_LEAF_PRE(SIZE) \
                    level_fixed_leafs = LeafCounts::from_size<SIZE>(); \
                    stored_size_size = SIZE; \
                    min_byte_size = max_byte_size = (SIZE).byte_size();

                    if (delta <= UINT8_MAX) {
                        ADD_SIZE_LEAF_PRE(SIZE::SIZE_1);
                        if (max_length <= UINT8_MAX) {
                            size_size = SIZE::SIZE_1;
                        } else if (max_length <= UINT16_MAX) {
                            size_size = SIZE::SIZE_2;
                        } else {
                            size_size = SIZE::SIZE_4;
                        }
                    } else if (delta <= UINT16_MAX) {
                        ADD_SIZE_LEAF_PRE(SIZE::SIZE_2);
                        if (max_length <= UINT16_MAX) {
                            size_size = SIZE::SIZE_2;
                        } else {
                            size_size = SIZE::SIZE_4;
                        }
                    } else /* if (delta <= UINT32_MAX) */ {
                        ADD_SIZE_LEAF_PRE(SIZE::SIZE_4);
                        size_size = SIZE::SIZE_4;
                    }
                    #undef ADD_SIZE_LEAF_PRE

                    min_byte_size += min_length;
                    max_byte_size += max_length;
                    StringType::create(buffer, min_length, stored_size_size, size_size);

                    return LexTypeResult{
                        lex_argument_list_end(cursor),
                        level_fixed_leafs,
                        LeafCounts::from_size<SIZE::SIZE_1>(),
                        min_byte_size,
                        max_byte_size,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        1,
                        stored_size_size
                    };
                },
                buffer
            );

        } else {
            return lex_range_argument<LexFixedTypeResult, false, true>(
                YYCURSOR,
                [](const char* cursor, uint32_t length, Buffer& buffer)->LexFixedTypeResult {
                    FixedStringType::create(buffer, length);

                    return LexFixedTypeResult{
                        lex_argument_list_end(cursor),
                        LeafCounts::from_size<SIZE::SIZE_1>(),
                        length,
                        0,
                        0,
                        0,
                        0,
                        0,
                        SIZE::SIZE_1
                    };
                },
                [typename_start] [[noreturn]] (const char* cursor)->LexFixedTypeResult {
                    show_syntax_error("expected fixed size string", typename_start, cursor - 1);
                },
                buffer
            );

        }
    }

    array: {
        auto [extended_idx, base_idx] = ArrayType::create(buffer);

        YYCURSOR = lex_argument_list_start(YYCURSOR);

        auto result = lex_type<true>(YYCURSOR, buffer, identifier_map);
        YYCURSOR = result.cursor;

        YYCURSOR = lex_symbol<',', "expected length argument">(YYCURSOR);
        
        if constexpr (!expect_fixed) {
            return lex_range_argument<LexTypeResult>(
                YYCURSOR,
                [](
                    const char* cursor,
                    uint32_t length,
                    LexFixedTypeResult&& result,
                    Type* base,
                    ArrayType* extended
                )->LexTypeResult {
                    *base = {ARRAY_FIXED};
                    *extended = {
                        result.level_fixed_leafs,
                        length,
                        static_cast<uint16_t>(-1),
                        SIZE::SIZE_0,
                        get_size_size(length)
                    };

                    uint64_t byte_size = length * result.byte_size;

                    return LexTypeResult{
                        lex_argument_list_end(cursor),
                        LeafCounts::zero(),
                        LeafCounts::zero(),
                        byte_size,
                        byte_size,
                        0,
                        1,
                        result.level_variant_fields,
                        gsl::narrow_cast<uint16_t>(result.level_fixed_leafs.total() + result.sublevel_fixed_leafs),
                        gsl::narrow_cast<uint16_t>(result.pack_count + 4),
                        0,
                        0,
                        result.alignment
                    };
                },
                [](
                    const char* cursor,
                    uint32_t min_length,
                    uint32_t max_length,
                    LexFixedTypeResult&& result,
                    Type* base,
                    ArrayType* extended
                )->LexTypeResult {
                    *base = {ARRAY};
                    uint32_t delta = max_length - min_length;

                    LeafCounts level_fixed_leafs;
                    uint64_t min_byte_size;
                    uint64_t max_byte_size;
                    SIZE alignment;

                    SIZE size_size;
                    SIZE stored_size_size;

                    #define ADD_SIZE_LEAF_PRE(SIZE) \
                    level_fixed_leafs = LeafCounts::from_size<SIZE>(); \
                    stored_size_size = SIZE; \
                    min_byte_size = max_byte_size = (SIZE).byte_size();

                    if (delta <= UINT8_MAX) {
                        ADD_SIZE_LEAF_PRE(SIZE::SIZE_1)
                        alignment = result.alignment;
                        if (max_length <= UINT8_MAX) {
                            size_size = SIZE::SIZE_1;
                        } else if (max_length <= UINT16_MAX) {
                            size_size = SIZE::SIZE_2;
                        } else {
                            size_size = SIZE::SIZE_4;
                        }
                    } else if (delta <= UINT16_MAX) {
                        ADD_SIZE_LEAF_PRE(SIZE::SIZE_2)
                        alignment = std::max(result.alignment, SIZE::SIZE_2);
                        if (max_length <= UINT16_MAX) {
                            size_size = SIZE::SIZE_2;
                        } else {
                            size_size = SIZE::SIZE_4;
                        }
                    } else /* if (delta <= UINT32_MAX) */ {
                        ADD_SIZE_LEAF_PRE(SIZE::SIZE_4)
                        alignment = std::max(result.alignment, SIZE::SIZE_4);
                        size_size = SIZE::SIZE_4; 
                    }
                    #undef ADD_SIZE_LEAF_PRE

                    min_byte_size += result.byte_size * min_length;
                    max_byte_size += result.byte_size * max_length;

                    *extended = {
                        result.level_fixed_leafs,
                        min_length,
                        static_cast<uint16_t>(-1),
                        stored_size_size,
                        size_size
                    };

                    return LexTypeResult{
                        lex_argument_list_end(cursor),
                        LeafCounts::zero(),
                        result.level_fixed_leafs,
                        min_byte_size,
                        max_byte_size,
                        result.level_fixed_variants,
                        1,
                        result.level_variant_fields,
                        gsl::narrow_cast<uint16_t>(result.level_fixed_leafs.total() + result.sublevel_fixed_leafs),
                        gsl::narrow_cast<uint16_t>(result.pack_count + 4),
                        0,
                        1,
                        alignment
                    };
                },
                std::move(result),
                buffer.get(base_idx),
                buffer.get(extended_idx)
            );
        } else {
            return lex_range_argument<LexFixedTypeResult, false, true>(
                YYCURSOR, 
                [](
                    const char* cursor,
                    uint32_t length,
                    LexFixedTypeResult&& result,
                    Type* base,
                    ArrayType* extended
                )->LexFixedTypeResult {
                    *base = {ARRAY_FIXED};
                    *extended = ArrayType{
                        result.level_fixed_leafs,
                        length,
                        static_cast<uint16_t>(-1),
                        SIZE::SIZE_0,
                        get_size_size(length)
                    };

                    return LexFixedTypeResult{
                        lex_argument_list_end(cursor),
                        LeafCounts::zero(),
                        result.byte_size * length,
                        0,
                        1,
                        result.level_variant_fields,
                        gsl::narrow_cast<uint16_t>(result.level_fixed_leafs.total() + result.sublevel_fixed_leafs),
                        gsl::narrow_cast<uint16_t>(result.pack_count + 4),
                        result.alignment
                    };
                },
                [typename_start] [[noreturn]] (const char* cursor)->LexFixedTypeResult {
                    show_syntax_error("expected fixed size array", typename_start, cursor - 1);
                },
                std::move(result),
                buffer.get(base_idx),
                buffer.get(extended_idx)
            );
        }
    }

    variant: {
        YYCURSOR = lex_argument_list_start(YYCURSOR);

        auto created_variant_type = DynamicVariantType::create(buffer);

        using bufferd_type_meta_t = variant_type_meta_t<!expect_fixed>;
        Buffer type_meta_buffer = BUFFER_INIT_STACK((BUFFER_INIT_ARRAY_SIZE<bufferd_type_meta_t, 8>));

        return lex_variant_types<false, expect_fixed, bufferd_type_meta_t>(
            YYCURSOR,
            UINT64_MAX,
            0,
            buffer,
            std::move(type_meta_buffer),
            identifier_map,
            created_variant_type,
            0,
            0,
            0,
            0,
            SIZE::SIZE_1
        );
    }

    identifier: {
        const char* const typename_end = YYCURSOR;

        auto identifier_idx_iter = identifier_map.find(std::string_view{typename_start, typename_end});
        if (identifier_idx_iter == identifier_map.end()) {
            show_syntax_error("identifier not defined", typename_start, typename_end - 1);
        }
        auto identifier_index = identifier_idx_iter->second;
        IdentifiedType::create(buffer, identifier_index);

        const IdentifiedDefinition* const identifier = buffer.get(identifier_index);
        switch (identifier->keyword)
        {
        case STRUCT: {
            const StructDefinition* const struct_definition = identifier->data()->as_struct();
            if constexpr (!expect_fixed) {
                return LexTypeResult{
                    YYCURSOR,
                    struct_definition->level_fixed_leafs,
                    struct_definition->var_leaf_counts,
                    struct_definition->min_byte_size,
                    struct_definition->max_byte_size,
                    struct_definition->level_fixed_variants,
                    struct_definition->level_fixed_arrays,
                    struct_definition->level_variant_fields,
                    struct_definition->sublevel_fixed_leafs,
                    struct_definition->pack_count,
                    struct_definition->total_variant_var_leafs,
                    struct_definition->level_size_leafs,
                    struct_definition->max_alignment
                };
            } else {
                if (!struct_definition->var_leaf_counts.empty()) {
                    show_syntax_error("fixed size struct expected", typename_start, YYCURSOR - 1);
                }
                return LexFixedTypeResult{
                    YYCURSOR,
                    struct_definition->level_fixed_leafs,
                    struct_definition->min_byte_size,
                    struct_definition->level_fixed_variants,
                    struct_definition->level_fixed_arrays,
                    struct_definition->level_variant_fields,
                    struct_definition->sublevel_fixed_leafs,
                    struct_definition->pack_count,
                    struct_definition->max_alignment
                };
            }

        }
        case ENUM: {
            const EnumDefinition* enum_definition = identifier->data()->as_enum();
            SIZE type_size = enum_definition->type_size;
            LeafCounts level_fixed_leafs {type_size};
            uint64_t byte_size = type_size.byte_size();
            if constexpr (!expect_fixed) {
                return LexTypeResult{
                    YYCURSOR,
                    level_fixed_leafs,
                    LeafCounts::zero(),
                    byte_size,
                    byte_size,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    type_size
                };
            } else {
                return LexFixedTypeResult{
                    YYCURSOR,
                    level_fixed_leafs,
                    byte_size,
                    0,
                    0,
                    0,
                    0,
                    0,
                    type_size
                };
            }
        }
        default:
            INTERNAL_ERROR("unreachable");
        }
    }
}

template <bool is_first_field>
inline const char* lex_struct_fields (
    const char* YYCURSOR,
    Buffer::Index<StructDefinition> definition_data_idx,
    IdentifierMap &identifier_map,
    Buffer &buffer,
    LeafCounts level_fixed_leafs,
    LeafCounts var_leaf_counts,
    uint64_t min_byte_size,
    uint64_t max_byte_size,
    uint16_t level_size_leafs,
    uint16_t level_fixed_variants,
    uint16_t level_fixed_arrays,
    uint16_t level_variant_fields,
    uint16_t sublevel_fixed_leafs,
    uint16_t total_variant_var_leafs,
    uint16_t field_count,
    uint16_t pack_count,
    SIZE max_alignment
) {
    before_field:
    /*!local:re2c
        any_white_space* [a-zA-Z_]   { goto name_start; }
        any_white_space* "}"         { goto struct_end; }

        any_white_space* { show_syntax_error("Expected field name or end of struct", YYCURSOR - 1); }
    */

    struct_end: {
        if constexpr (is_first_field) {
            show_syntax_error("expected at least one field", YYCURSOR - 1);
        } else {
            StructDefinition* const definition_data = buffer.get(definition_data_idx);
            *definition_data = StructDefinition{
                {definition_data->name}, // TODO: fix this clanky shit
                level_fixed_leafs,
                var_leaf_counts,
                min_byte_size,
                max_byte_size,
                level_size_leafs,
                level_fixed_variants,
                level_fixed_arrays,
                level_variant_fields,
                sublevel_fixed_leafs,
                total_variant_var_leafs,
                field_count,
                pack_count,
                max_alignment
            };
            return YYCURSOR;
        }
    }

    name_start:
    const char* start = YYCURSOR - 1;
    /*!local:re2c
        [a-zA-Z0-9_]*  { goto name_end; }
    */
    name_end:
    const char* end = YYCURSOR;
    size_t length = end - start;
    if constexpr (!is_first_field) {
        buffer.get(definition_data_idx)->visit_uninitialized([&](const lexer::StructField::Data* const field_data) {
            if (string_view_equal(field_data->name, start, length)) {
                show_syntax_error("field already defined", start, length);
            }
            return field_data->type()->skip<const StructField>();
        }, field_count);
    }
    /*!local:re2c
        any_white_space* ":" { goto struct_field; }
        any_white_space* { UNEXPECTED_INPUT("expected ':'"); }
    */

    struct_field: {
        StructField::Data* field = StructDefinition::reserve_field(buffer);
        *field = {{start, length}};

        auto result = lex_type<false>(YYCURSOR, buffer, identifier_map);
        YYCURSOR = result.cursor;

        YYCURSOR = lex_symbol<';'>(YYCURSOR);

        if constexpr (is_first_field) {
            return lex_struct_fields<false>(
                YYCURSOR,
                definition_data_idx,
                identifier_map,
                buffer,
                result.level_fixed_leafs,
                result.var_leaf_counts,
                result.min_byte_size,
                result.max_byte_size,
                result.level_size_leafs,
                result.level_fixed_variants,
                result.level_fixed_arrays,
                result.level_variant_fields,
                result.sublevel_fixed_leafs,
                result.total_variant_var_leafs,
                1,
                result.pack_count,
                result.alignment
            );
        } else {
            field_count++;
            level_fixed_leafs += result.level_fixed_leafs;
            var_leaf_counts += result.var_leaf_counts;
            min_byte_size += result.min_byte_size;
            max_byte_size += result.max_byte_size;
            level_size_leafs += result.level_size_leafs;
            sublevel_fixed_leafs += result.sublevel_fixed_leafs;
            total_variant_var_leafs += result.total_variant_var_leafs;
            level_fixed_variants += result.level_fixed_variants;
            level_fixed_arrays += result.level_fixed_arrays;
            level_variant_fields += result.level_variant_fields;
            pack_count += result.pack_count;
            max_alignment = std::max(max_alignment, result.alignment);
            goto before_field;
        }
    }

}

inline const char* lex_struct(
    const char* YYCURSOR,
    Buffer::Index<StructDefinition> definition_data_idx,
    IdentifierMap &identifier_map,
    Buffer &buffer
) {
    YYCURSOR = lex_symbol<'{', "Expected '{' to denote start of struct">(YYCURSOR);

    return lex_struct_fields<true>(
        YYCURSOR,
        definition_data_idx,
        identifier_map,
        buffer,
        LeafCounts::zero(),
        LeafCounts::zero(),
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        SIZE::SIZE_1
    );
}



template <bool is_signed>
inline const char* lex_enum_fields (
    const char* YYCURSOR,
    Buffer::Index<EnumDefinition> definition_data_idx,
    uint16_t field_count,
    EnumField::Value value,
    uint64_t max_value_unsigned,
    boost::unordered::unordered_flat_set<std::string_view>&& member_names,
    IdentifierMap& identifier_map,
    Buffer &buffer
) {
    while (true) {
        /*!local:re2c
            any_white_space* [a-zA-Z_]   { goto name_start; }
            any_white_space* "}"         { goto enum_end; }

            any_white_space* { show_syntax_error("Expected field name or end of struct", YYCURSOR - 1); }
        */

        enum_end: {
            if (field_count == 0) {
                show_syntax_error("expected at least one member", YYCURSOR - 1);
            }
            SIZE type_size;
            if (max_value_unsigned <= UINT8_MAX) {
                type_size = SIZE::SIZE_1;
            } else if (max_value_unsigned <= UINT16_MAX) {
                type_size = SIZE::SIZE_2;
            } else if (max_value_unsigned <= UINT32_MAX) {
                type_size = SIZE::SIZE_4;
            } else {
                type_size = SIZE::SIZE_8;
            }
            EnumDefinition* const definition_data = buffer.get(definition_data_idx);
            *definition_data = {
                {definition_data->name},
                field_count,
                type_size
            };
            return YYCURSOR;
        }

        name_start:
        const char* start = YYCURSOR - 1;
        /*!local:re2c
            [a-zA-Z0-9_]*  { goto name_end; }
        */
        name_end:
        std::string_view name{start, YYCURSOR};
        {   
            bool did_emplace = member_names.emplace(name).second;
            if (!did_emplace) {
                show_syntax_error("field already defined", name.data(), name.size());
            }
        }
        /*!local:re2c
            any_white_space* "," { goto default_value; }
            any_white_space* "=" { goto custom_value; }
            any_white_space* "}" { goto default_last_member; }
            any_white_space* { UNEXPECTED_INPUT("expected custom value or ','"); }
        */

        default_value: {
            value.increment();
            goto enum_member;
        }

        custom_value: {
            /*!local:re2c
                any_white_space* "-"    { goto parse_negative_value; }
                any_white_space*        { goto parse_positive_value; }
            */

            parse_negative_value: {
                auto parsed = parse_uint<uint64_t, false, uint64_t{0} - static_cast<uint64_t>(std::numeric_limits<int64_t>::min())>(YYCURSOR);
                YYCURSOR = parsed.cursor;


                max_value_unsigned = std::max(max_value_unsigned, (parsed.value * 2) - 1);

                bool is_negative = parsed.value != 0;
                value = {parsed.value, is_negative};

                /*!local:re2c
                    any_white_space* "," { goto enum_member_signed; }
                    any_white_space* "}" { goto last_member; }
                    any_white_space* { UNEXPECTED_INPUT("expected ',' or end of enum definition"); }
                */
                enum_member_signed: {
                    field_count++;
                    EnumDefinition::add_field(buffer, {name, value});
                    return lex_enum_fields<true>(YYCURSOR, definition_data_idx, field_count, value, max_value_unsigned, std::move(member_names), identifier_map, buffer);
                }
            }

            parse_positive_value: {
                auto parsed = parse_uint<
                    uint64_t,
                    false,
                    std::numeric_limits<std::conditional_t<
                        is_signed,
                        int64_t,
                        uint64_t
                    >>::max()
                >(YYCURSOR);
                YYCURSOR = parsed.cursor;
                if constexpr (is_signed) {
                    max_value_unsigned = std::max(max_value_unsigned, (parsed.value * 2) - 1);
                } else {
                    max_value_unsigned = std::max(max_value_unsigned, parsed.value);
                }

                value = EnumField::Value{parsed.value, false};

                /*!local:re2c
                    any_white_space* "," { goto enum_member; }
                    any_white_space* "}" { goto last_member; }
                    any_white_space* { UNEXPECTED_INPUT("expected ',' or end of enum definition"); }
                */
            }
        }

        default_last_member: {
            value.increment();
            goto last_member;
        }

        last_member: {
            field_count++;
            EnumDefinition::add_field(buffer, {name, value});
            goto enum_end;
        }

        enum_member: {
            field_count++;
            EnumDefinition::add_field(buffer, {name, value});
        }
    }
}

inline const char* lex_enum (
    const char* YYCURSOR,
    Buffer::Index<EnumDefinition> definition_data_idx,
    IdentifierMap &identifier_map,
    Buffer &buffer
) {
    YYCURSOR = lex_symbol<'{', "Expected '{' to denote start of enum">(YYCURSOR);
    return lex_enum_fields<false>(YYCURSOR, definition_data_idx, 0, EnumField::Value::intitial(), 0, {}, identifier_map, buffer);
}


template <bool target_defined>
inline const StructDefinition* lex (const char* YYCURSOR, IdentifierMap &identifier_map, Buffer &buffer, std::conditional_t<target_defined, const StructDefinition*, estd::empty> target) {
    loop: {
    /*!local:re2c

        any_white_space* "struct"  any_white_space  { goto struct_keyword; }
        any_white_space* "enum"    any_white_space  { goto enum_keyword; }
        any_white_space* "target"  any_white_space  { goto target_keyword; }

        any_white_space* [\x00]                     { goto eof; }

        any_white_space*                            { UNEXPECTED_INPUT("unexpected input"); }

    */
    goto loop;
    }

    struct_keyword: {
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        auto [definition_data_idx, definition_idx] = StructDefinition::create(buffer);
        *buffer.get(definition_idx) = {KEYWORDS::STRUCT};
        buffer.get(definition_data_idx)->name = name_result.value;
        YYCURSOR = lex_struct(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(identifier_map, name_result.value, definition_idx);
        if constexpr (target_defined) {
            console.warn("no possible path from target to struct ", name_result.value, " can be created.");
        }
        goto loop;
    }
    enum_keyword: {
        auto name_result = lex_identifier_name(YYCURSOR);
        YYCURSOR = name_result.cursor;
        auto [definition_data_idx, definition_idx] = EnumDefinition::create(buffer);
        *buffer.get(definition_idx) = {KEYWORDS::ENUM};
        buffer.get(definition_data_idx)->name = name_result.value;
        YYCURSOR = lex_enum(YYCURSOR, definition_data_idx, identifier_map, buffer);
        add_identifier(identifier_map, name_result.value, definition_idx);
        if constexpr (target_defined) {
            console.warn("no possible path from target to enum ", name_result.value, " can be created.");
        }
        goto loop;
    }
    target_keyword: {
        if constexpr (target_defined) {
            show_syntax_error("target already defined", YYCURSOR);
        } else {
            auto type_idx = Buffer::Index<Type>{buffer.current_position()};
            YYCURSOR = lex_type<false>(YYCURSOR, buffer, identifier_map).cursor;
            YYCURSOR = lex_symbol<';'>(YYCURSOR);
            Type* const type = buffer.get(type_idx);
            if (type->type != FIELD_TYPE::IDENTIFIER) {
                INTERNAL_ERROR("target must be an identifier");
            }
            auto identifier_idx = type->as_identifier()->identifier_idx;
            const IdentifiedDefinition* const identified_definition = buffer.get(identifier_idx);
            if (identified_definition->keyword != KEYWORDS::STRUCT) {
                INTERNAL_ERROR("target must be a struct");
            }
            return lex<true>(YYCURSOR, identifier_map, buffer, identified_definition->data()->as_struct());
        }
    }

    eof: {
        if constexpr (target_defined) {
            return target;
        } else {
            INTERNAL_ERROR("target not defined");
        }
    }
}

}