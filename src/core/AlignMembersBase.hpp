#pragma once

#include <utility>
#include <type_traits>

#include "../estd/utility.hpp"
#include "./SIZE.hpp"

template<typename T, SIZE max_alignment = SIZE::SIZE_8, SIZE min_alignment = SIZE::SIZE_1, typename Outside = void>
requires (max_alignment != SIZE::SIZE_0 && min_alignment != SIZE::SIZE_0)
struct AlignMembersBase {
    static_assert(max_alignment >= min_alignment, "max_alignment can not be smaller than min_alignment");
    
    using alignments = make_size_range<min_alignment, max_alignment>;

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

    template <SIZE... alignments>
    constexpr explicit AlignMembersBase (estd::variadic_v<alignments...> /*unused*/)
        : align{(static_cast<void>(alignments), T{})...} {}
    
public:
    constexpr explicit AlignMembersBase () requires (std::is_default_constructible_v<T>)
        : AlignMembersBase{alignments{}} {}

    constexpr explicit AlignMembersBase (const T (&align)[alignments_count])
        : align(align) {}

    /**
     * @param aligns Values for alignments in ascending order.
     */
    template <typename... U>
    requires (alignments_count > 1 && sizeof...(U) == alignments_count)
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr AlignMembersBase (U&&... aligns)
        : align{std::forward<U>(aligns) ...} {}

    template <typename U>
    requires (alignments_count == 1)
    constexpr explicit AlignMembersBase (U&& align)
        : align{std::forward<U>(align)} {}

    #define IDENTITY(...) __VA_ARGS__

    #define ALIGN_MEMBER_GET_CT_ARG(CONST_ATTR)                                     \
    template <SIZE alignment>                                                       \
    [[nodiscard]] constexpr CONST_ATTR IDENTITY(T&) get () IDENTITY(CONST_ATTR) {   \
        static_assert(alignment <= max_alignment && alignment >= min_alignment);    \
        return align[(alignment - min_alignment).ordinal()];                        \
    }

    #define ALIGN_MEMBER_GET_RT_ARG(CONST_ATTR)                                                 \
    template <estd::discouraged_annotation>                                                     \
    [[nodiscard]] constexpr CONST_ATTR IDENTITY(T&) get (SIZE alignment) IDENTITY(CONST_ATTR) { \
        BSSERT(alignment <= max_alignment && alignment >= min_alignment);                       \
        return align[(alignment - min_alignment).ordinal()];                                    \
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
    template <typename Result, SIZE... alignments>
    [[nodiscard]] static constexpr Result _of (const T& value, estd::variadic_v<alignments...> /*unused*/) { return Result{(static_cast<void>(alignments), value)...}; }

public:
    template <typename Result = outside_t>
    [[nodiscard]] static constexpr Result of (const T& value) { return _of<Result>(value, alignments{}); }
};