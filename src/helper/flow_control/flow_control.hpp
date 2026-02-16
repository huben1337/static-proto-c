#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include "../../estd/utility.hpp"
#include "../../estd/empty.hpp"
#include "./control_kind.hpp"

namespace flow_control {

struct FlowControlFeatures {
    consteval FlowControlFeatures (bool none, bool continue_, bool break_, bool return_)
        : has_none(none), has_continue(continue_), has_break(break_), has_return(return_) {}

    bool has_none;
    bool has_continue;
    bool has_break;
    bool has_return;
};

namespace detail {

template <FlowControlFeatures features>
constexpr size_t enabled_feature_count
    = static_cast<int>(features.has_none)
    + static_cast<int>(features.has_continue)
    + static_cast<int>(features.has_break)
    + static_cast<int>(features.has_return);

enum class TAGGING_STRATEGY : uint8_t {
    INVARIANT_ONLY,
    INVARIANT_WITH_BOOL,
    ENUM
};

struct TaggingInfo {
    consteval TaggingInfo (TAGGING_STRATEGY tagging_strategy, bool bool_tag, size_t invariant_idx)
        : tagging_strategy(tagging_strategy), bool_tag(bool_tag), invariant_idx(invariant_idx) {}

    TAGGING_STRATEGY tagging_strategy;
    bool bool_tag;
    size_t invariant_idx;
};

template <TAGGING_STRATEGY tagging_strategy>
constexpr TaggingInfo intitial_tagging_info {
    tagging_strategy,
    false,
    0
};

template <TaggingInfo tagging_info>
constexpr TaggingInfo next_tagging_info {
    tagging_info.tagging_strategy,
    !tagging_info.bool_tag,
    tagging_info.invariant_idx
        + (tagging_info.tagging_strategy == TAGGING_STRATEGY::INVARIANT_ONLY 
            || (tagging_info.tagging_strategy == TAGGING_STRATEGY::INVARIANT_WITH_BOOL && tagging_info.bool_tag)
                ? 1
                : 0)
};


template <FlowControlFeatures features>
using control_kind_for_features_t = ControlKind_<features.has_none, features.has_continue, features.has_break, features.has_return>::ControlKind;

using UnrestrictedControlKind = ControlKind_<true, true, true, true>::ControlKind;

template <typename ConrolKind, UnrestrictedControlKind kind>
constexpr ConrolKind mapped_kind_v;
template <typename ConrolKind>
constexpr ConrolKind mapped_kind_v<ConrolKind, UnrestrictedControlKind::NONE> = ConrolKind::NONE;
template <typename ConrolKind>
constexpr ConrolKind mapped_kind_v<ConrolKind, UnrestrictedControlKind::CONTINUE> = ConrolKind::CONTINUE;
template <typename ConrolKind>
constexpr ConrolKind mapped_kind_v<ConrolKind, UnrestrictedControlKind::BREAK> = ConrolKind::BREAK;


struct flow_control_constant_ {
    template <typename Derived, UnrestrictedControlKind kind, TAGGING_STRATEGY, TaggingInfo tagging_info, typename Invariants>
    struct flow_control_constant;

    template <typename Derived, UnrestrictedControlKind kind, TaggingInfo tagging_info, typename Invariants>
    struct flow_control_constant<Derived, kind, TAGGING_STRATEGY::INVARIANT_ONLY, tagging_info, Invariants> {
        static constexpr Derived value {Invariants::template nth_v<tagging_info.invariant_idx>};
    };

    template <typename Derived, UnrestrictedControlKind kind, TaggingInfo tagging_info, typename Invariants>
    struct flow_control_constant<Derived, kind, TAGGING_STRATEGY::INVARIANT_WITH_BOOL, tagging_info, Invariants> {
        static constexpr Derived value {Invariants::template nth_v<tagging_info.invariant_idx>, tagging_info.bool_tag};
    };

    template <typename Derived, UnrestrictedControlKind kind, TaggingInfo tagging_info, typename Invariants>
    struct flow_control_constant<Derived, kind, TAGGING_STRATEGY::ENUM, tagging_info, Invariants> {
        static constexpr Derived value {mapped_kind_v<typename Derived::kind_t, kind>};
    };
};

template <typename Derived, UnrestrictedControlKind kind, TaggingInfo tagging_info, typename Invariants>
constexpr Derived flow_control_v = flow_control_constant_::flow_control_constant<Derived, kind, tagging_info.tagging_strategy, tagging_info, Invariants>::value;

#define RESTRICT_FLOW_CONTROL_BASE_CTOR_FOR(OWN_NAME, FRIEND_NAME) \
public: \
    template <typename, typename T_, FlowControlFeatures, bool, TaggingInfo, T_...> \
    friend struct FRIEND_NAME; \
    friend Derived; \
private: \
    consteval OWN_NAME () = default; \
public: 

struct ReturnFlowControlBase_ {
    template <
        typename Derived,
        typename T,
        FlowControlFeatures features,
        bool has_return,
        TaggingInfo tagging_info,
        T... invariants
    >
    struct ReturnFlowControlBase {
        RESTRICT_FLOW_CONTROL_BASE_CTOR_FOR(ReturnFlowControlBase, BreakFlowControlBase)
    };

    template <
        typename Derived,
        typename T,
        FlowControlFeatures features,
        TaggingInfo tagging_info,
        T... invariants
    >
    struct ReturnFlowControlBase<
        Derived,
        T,
        features,
        true,
        tagging_info,
        invariants...
    > {
        RESTRICT_FLOW_CONTROL_BASE_CTOR_FOR(ReturnFlowControlBase, BreakFlowControlBase)

        template <typename U>
        [[nodiscard]] static constexpr Derived return_ (U&& u) {
            if constexpr (tagging_info.tagging_strategy == TAGGING_STRATEGY::INVARIANT_ONLY) {
                return {std::forward<U>(u), estd::empty{}};
            } else if constexpr (tagging_info.tagging_strategy == TAGGING_STRATEGY::INVARIANT_WITH_BOOL) {
                return {std::forward<U>(u), false};
            } else if constexpr (tagging_info.tagging_strategy == TAGGING_STRATEGY::ENUM) {
                return {std::forward<U>(u), Derived::kind_t::RETURN};
            } else {
                static_assert(false);
            }
        }

        template <T value>
        [[nodiscard]] static consteval Derived return_ () {
            static_assert(((value != invariants) && ...));
            return return_(value);
        }
    };

    template <
        typename Derived,
        FlowControlFeatures features,
        TaggingInfo tagging_info
    >
    struct ReturnFlowControlBase<
        Derived,
        void,
        features,
        true,
        tagging_info
    > {
        RESTRICT_FLOW_CONTROL_BASE_CTOR_FOR(ReturnFlowControlBase, BreakFlowControlBase)

        [[nodiscard]] static constexpr Derived return_ () {
            if constexpr (tagging_info.tagging_strategy == TAGGING_STRATEGY::ENUM) {
                return Derived{Derived::kind_t::RETURN};
            } else {
                static_assert(false);
            }
        }
    };
};

template <
    typename Derived,
    typename T,
    FlowControlFeatures features,
    bool has_break,
    TaggingInfo tagging_info,
    T... invariants
>
struct BreakFlowControlBase : ReturnFlowControlBase_::ReturnFlowControlBase<
    Derived,
    T,
    features,
    features.has_return,
    tagging_info,
    invariants...
> {
    RESTRICT_FLOW_CONTROL_BASE_CTOR_FOR(BreakFlowControlBase, ContinueFlowControlBase)
};

template <
    typename Derived,
    typename T,
    FlowControlFeatures features,
    TaggingInfo tagging_info,
    T... invariants
>
struct BreakFlowControlBase<
    Derived,
    T,
    features,
    true,
    tagging_info,
    invariants...
> : ReturnFlowControlBase_::ReturnFlowControlBase<
    Derived,
    T,
    features,
    features.has_return,
    next_tagging_info<tagging_info>,
    invariants...
> {
    RESTRICT_FLOW_CONTROL_BASE_CTOR_FOR(BreakFlowControlBase, ContinueFlowControlBase)

    [[nodiscard]] static consteval Derived break_ () {
        static constexpr Derived v = flow_control_v<Derived, UnrestrictedControlKind::BREAK, tagging_info, estd::variadic_v<invariants...>>;
        return v;
    }
};

template <
    typename Derived,
    typename T,
    FlowControlFeatures features,
    bool has_continue,
    TaggingInfo tagging_info,
    T... invariants
>
struct ContinueFlowControlBase : BreakFlowControlBase<
    Derived,
    T,
    features,
    features.has_break,
    tagging_info,
    invariants...
> {
    RESTRICT_FLOW_CONTROL_BASE_CTOR_FOR(ContinueFlowControlBase, NoneFlowControlBase)
};

template <
    typename Derived,
    typename T,
    FlowControlFeatures features,
    TaggingInfo tagging_info,
    T... invariants
>
struct ContinueFlowControlBase<
    Derived,
    T,
    features,
    true,
    tagging_info,
    invariants...
> : BreakFlowControlBase<
    Derived,
    T,
    features,
    features.has_break,
    next_tagging_info<tagging_info>,
    invariants...
> {
    RESTRICT_FLOW_CONTROL_BASE_CTOR_FOR(ContinueFlowControlBase, NoneFlowControlBase)

    [[nodiscard]] static consteval Derived continue_ () {
        static constexpr Derived v = flow_control_v<Derived, UnrestrictedControlKind::CONTINUE, tagging_info, estd::variadic_v<invariants...>>;
        return v;
    }
};

template <
    typename Derived,
    typename T,
    FlowControlFeatures features,
    bool has_none,
    TaggingInfo tagging_info,
    T... invariants
>
struct NoneFlowControlBase : ContinueFlowControlBase<
    Derived,
    T,
    features,
    features.has_continue,
    tagging_info,
    invariants...
> {
    friend struct InvariantBasedFlowControlBase_;
    friend Derived;

private:
    consteval NoneFlowControlBase() = default;
};


template <
    typename Derived,
    typename T,
    FlowControlFeatures features,
    TaggingInfo tagging_info,
    T... invariants
>
struct NoneFlowControlBase<
    Derived,
    T,
    features,
    true,
    tagging_info,
    invariants...
> : ContinueFlowControlBase<
    Derived,
    T,
    features,
    features.has_continue,
    next_tagging_info<tagging_info>,
    invariants...
> {
    friend struct InvariantBasedFlowControlBase_;
    friend Derived;

private:
    consteval NoneFlowControlBase() = default;

public:
    [[nodiscard]] static consteval Derived none_ () {
        static constexpr Derived v = flow_control_v<Derived, UnrestrictedControlKind::NONE, tagging_info, estd::variadic_v<invariants...>>;
        return v;
    }
};

#undef RESTRICT_FLOW_CONTROL_BASE_CTOR_FOR

struct InvariantBasedFlowControlBase_ {
    template <
        typename Derived,
        typename T,
        FlowControlFeatures features_,
        TAGGING_STRATEGY tagging_strategy_,
        T... invariants
    >
    struct InvariantBasedFlowControlBase : NoneFlowControlBase<
        Derived,
        T,
        features_,
        features_.has_none,
        intitial_tagging_info<tagging_strategy_>,
        invariants...
    > {
        friend Derived;

        using kind_t = control_kind_for_features_t<features_>;
        using data_t = T;

        static constexpr TAGGING_STRATEGY tagging_strategy = tagging_strategy_;
        static constexpr FlowControlFeatures features = features_;

    private:
        consteval InvariantBasedFlowControlBase () = default;

        template <UnrestrictedControlKind kind>
        [[nodiscard]] static consteval Derived map_invariant () {
            if constexpr (kind == UnrestrictedControlKind::NONE) {
                return Derived::none_();
            } else if constexpr (kind == UnrestrictedControlKind::CONTINUE) {
                return Derived::continue_();
            } else if constexpr (kind == UnrestrictedControlKind::BREAK) {
                return Derived::break_();
            } else {
                static_assert(false);
            }
        }

        template<UnrestrictedControlKind... kinds>
        [[nodiscard, gnu::always_inline]] constexpr kind_t kind_ (this const Derived& self, estd::variadic_v<kinds...> /*unused*/) {
            kind_t result;

            const bool matched = ((self == map_invariant<kinds>() ? (result = mapped_kind_v<kind_t, kinds>, true) : false) || ...);
            if constexpr (features.has_return) {
                if (!matched) {
                    result = kind_t::RETURN;
                }
            }

            return result;
        }

        struct ControlKindConfig {
            bool enabled;
            UnrestrictedControlKind kind;
        };

        template <typename result, bool enabled, UnrestrictedControlKind kind>
        struct conditionaly_append_kind {
            using type = result;
        };

        template <typename result, UnrestrictedControlKind kind>
        struct conditionaly_append_kind<result, true, kind> {
            using type = typename result::template append<kind>;
        };

        template <typename result, ControlKindConfig kind_config>
        using conditionaly_append_kind_t = conditionaly_append_kind<result, kind_config.enabled, kind_config.kind>::type;

        template <typename result, ControlKindConfig first, ControlKindConfig... rest>
        struct used_kinds {
            using type = used_kinds<conditionaly_append_kind_t<result, first>, rest...>::type;
        };

        template <typename result, ControlKindConfig first>
        struct used_kinds<result, first> {
            using type = conditionaly_append_kind_t<result, first>;
        };

        using used_kinds_t = used_kinds<estd::variadic_v<>, {
            features.has_none,
            UnrestrictedControlKind::NONE
        }, {
            features.has_continue,
            UnrestrictedControlKind::CONTINUE
        }, {
            features.has_break,
            UnrestrictedControlKind::BREAK
        }>::type;

    public:
        [[nodiscard]] constexpr kind_t kind (this const Derived& self) {
            return self.kind_(used_kinds_t{});
        }

        [[nodiscard]] constexpr const T& data (this const Derived& self) { return self._data; }

        [[nodiscard]] constexpr T data (this Derived&& self) { return std::move(self)._data; }
    };
};

template <typename From, typename... To>
static constexpr bool is_convertable_to_any_of_v = (std::is_convertible_v<From, To> || ...);

template <
    typename T,
    FlowControlFeatures features,
    TAGGING_STRATEGY tagging_strategy,
    T... invariants
>
struct FlowControl;

struct InvariantOnlyIdProivder_ {
    template <typename, bool>
    struct InvariantOnlyIdProivder {
        static constexpr bool is_convertible_to_integral = false;
    };

    template <typename T>
    struct InvariantOnlyIdProivder<T, true> {
        static constexpr bool is_convertible_to_integral = true;

        [[nodiscard]] constexpr auto id (this const auto& self) {
            if constexpr (std::is_integral_v<T>) {
                return self._data;
            } else {
                if constexpr (std::is_convertible_v<T, int8_t> || std::is_convertible_v<T, uint8_t>) {
                    return static_cast<uint8_t>(self._data);
                } else if constexpr (std::is_convertible_v<T, int16_t> || std::is_convertible_v<T, uint16_t>) {
                    return static_cast<uint16_t>(self._data);
                } else if constexpr (std::is_convertible_v<T, int32_t> || std::is_convertible_v<T, uint32_t>) {
                    return static_cast<uint32_t>(self._data);
                } else if constexpr (std::is_convertible_v<T, int64_t> || std::is_convertible_v<T, uint64_t>) {
                    return static_cast<uint64_t>(self._data);
                } else if constexpr (std::is_convertible_v<T, __int128_t> || std::is_convertible_v<T, __uint128_t>) {
                    return static_cast<__uint128_t>(self._data);
                } else {
                    static_assert(false);
                }
            }
        }
    };
};

template <
    typename T,
    FlowControlFeatures features,
    T... invariants
>
struct FlowControl<
    T,
    features,
    TAGGING_STRATEGY::INVARIANT_ONLY,
    invariants...
> : InvariantBasedFlowControlBase_::InvariantBasedFlowControlBase<
    FlowControl<T, features, TAGGING_STRATEGY::INVARIANT_ONLY, invariants...>,
    T,
    features,
    TAGGING_STRATEGY::INVARIANT_ONLY,
    invariants...
>, InvariantOnlyIdProivder_::InvariantOnlyIdProivder<T, is_convertable_to_any_of_v<T, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t, __uint128_t, __int128_t>>  {
    friend struct flow_control_constant_;
    friend struct InvariantBasedFlowControlBase_;
    friend struct ReturnFlowControlBase_;
    friend struct InvariantOnlyIdProivder_;
    
private:
    T _data;

    consteval explicit FlowControl (const T& data) : _data(data) {}

    template <typename U>
    constexpr FlowControl (U&& data, estd::empty /*unused*/) : _data(std::forward<U>(data)) {
        if !consteval {
            assert(((_data != invariants) && ...));
        }
    }

public:
    [[nodiscard]] constexpr bool operator == (this const FlowControl& self, const FlowControl& other) {
        return self._data == other._data;
    }
};

struct InvariantWithBoolIdProvider_ {
    template <typename, bool>
    struct InvariantWithBoolIdProvider {
        static constexpr bool is_convertible_to_integral = false;
    };

    template <typename T>
    struct InvariantWithBoolIdProvider<T, true> {
        static constexpr bool is_convertible_to_integral = true;

        [[nodiscard]] constexpr auto id (this const auto& self) {
            if constexpr (std::is_convertible_v<T, int8_t> || std::is_convertible_v<T, uint8_t>) {
                return static_cast<uint16_t>((self._data << 1) | self.bool_tag);
            } else if constexpr (std::is_convertible_v<T, int16_t> | std::is_convertible_v<T, uint16_t>) {
                return static_cast<uint32_t>((self._data << 1) || self.bool_tag);
            } else if constexpr (std::is_convertible_v<T, int32_t> | std::is_convertible_v<T, uint32_t>) {
                return static_cast<uint64_t>((self._data << 1) || self.bool_tag);
            } else if constexpr (std::is_convertible_v<T, int64_t> | std::is_convertible_v<T, uint64_t>) {
                return static_cast<__uint128_t>((self._data << 1) | self.bool_tag);
            } else {
                static_assert(false);
            }
        }
    };
};

template <
    typename T,
    FlowControlFeatures features,
    T... invariants
>
struct FlowControl<
    T,
    features,
    TAGGING_STRATEGY::INVARIANT_WITH_BOOL,
    invariants...
> : InvariantBasedFlowControlBase_::InvariantBasedFlowControlBase<
    FlowControl<T, features, TAGGING_STRATEGY::INVARIANT_WITH_BOOL, invariants...>,
    T,
    features,
    TAGGING_STRATEGY::INVARIANT_WITH_BOOL,
    invariants...
>, InvariantWithBoolIdProvider_::InvariantWithBoolIdProvider<T, is_convertable_to_any_of_v<T, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t>> {
    friend struct flow_control_constant_;
    friend struct InvariantBasedFlowControlBase_;
    friend struct ReturnFlowControlBase_;
    friend struct InvariantWithBoolIdProvider_;

private:
    T _data;
    bool bool_tag;

    template <typename U>
    constexpr FlowControl (U&& data, const bool bool_tag) : _data(std::forward<U>(data)), bool_tag(bool_tag)  {
        if !consteval {
            assert(((_data != invariants) && ...));
        }
    }

public:
    [[nodiscard]] constexpr bool operator == (this const FlowControl& self, const FlowControl& other) {
        return self.bool_tag == other.bool_tag
            && self._data    == other._data;
    }
};

template <
    typename T,
    FlowControlFeatures features_,
    T... invariants
>
struct FlowControl<
    T,
    features_,
    TAGGING_STRATEGY::ENUM,
    invariants...
> : NoneFlowControlBase<
    FlowControl<T, features_, TAGGING_STRATEGY::ENUM>,
    T,
    features_,
    features_.has_none,
    intitial_tagging_info<TAGGING_STRATEGY::ENUM>,
    invariants...
>  {
    friend struct flow_control_constant_;
    friend struct ReturnFlowControlBase_;

    using data_t = T;
    using kind_t = control_kind_for_features_t<features_>;

    static constexpr FlowControlFeatures features = features_;
    static constexpr TAGGING_STRATEGY tagging_strategy = TAGGING_STRATEGY::ENUM;

private:
    T _data;
    kind_t _kind;

    consteval explicit FlowControl (const kind_t& kind) : _data(), _kind(kind) {}

    template <typename U>
    constexpr FlowControl (U&& data, const kind_t& kind) : _data(std::forward<U>(data)), _kind(kind) {
        if !consteval {
            assert(((_data != invariants) && ...));
        }
    }

public:
    [[nodiscard]] constexpr const T& data (this const FlowControl& self) { return self._data; }

    [[nodiscard]] constexpr T data (this FlowControl&& self) { return std::move(self)._data; }

    [[nodiscard]] constexpr kind_t kind () const { return _kind; }
};

template <
    FlowControlFeatures features_
>
struct FlowControl<
    void,
    features_,
    TAGGING_STRATEGY::ENUM
> : NoneFlowControlBase<
    FlowControl<void, features_, TAGGING_STRATEGY::ENUM>,
    void,
    features_,
    features_.has_none,
    intitial_tagging_info<TAGGING_STRATEGY::ENUM>
> {
    friend struct flow_control_constant_;
    friend struct ReturnFlowControlBase_;

    using data_t = void;
    using kind_t = control_kind_for_features_t<features_>;

    static constexpr FlowControlFeatures features = features_;
    static constexpr TAGGING_STRATEGY tagging_strategy = TAGGING_STRATEGY::ENUM;

private:
    kind_t _kind;

    consteval explicit FlowControl (const kind_t& kind) : _kind(kind) {}

public:
    [[nodiscard]] constexpr kind_t kind () const { return _kind; }
};

}

template <
    FlowControlFeatures features,
    typename T = void,
    T... invariants
>
struct make_flow_control {
    static_assert(features.has_return, "Must use return flow control when type is not void");

private:
    static constexpr size_t feature_count = detail::enabled_feature_count<features>;
    static constexpr size_t invariant_count = sizeof...(invariants);

    static constexpr detail::TAGGING_STRATEGY tagging_strategy
        = feature_count - 1 <= invariant_count
            ? detail::TAGGING_STRATEGY::INVARIANT_ONLY
            : feature_count - 1 <= invariant_count * 2
                ? detail::TAGGING_STRATEGY::INVARIANT_WITH_BOOL
                : detail::TAGGING_STRATEGY::ENUM;

public:
    using type = detail::FlowControl<
        T,
        features,
        tagging_strategy,
        invariants...
    >;
};

template <
    FlowControlFeatures features
>
struct make_flow_control<features, void> {
    using type = detail::FlowControl<
        void,
        features,
        detail::TAGGING_STRATEGY::ENUM
    >;
};

template <typename FlowControl, auto first, auto ... rest, typename F>
constexpr auto controlled_for (F&& lambda) {
    static_assert(!FlowControl::features.has_none, "For loop control must not have `none` control kind.");
    static_assert(FlowControl::features.has_continue, "For loop control must have `continue` control kind.");
    static_assert(FlowControl::features.has_break, "For loop control must have `break` control kind.");
    constexpr bool allow_folding = true;
    constexpr bool prefer_folding = false;

    FlowControl flow_control = lambda.template operator()<first>();

    #define FOLD_REST(IS_NOT_CONTINUE_COMP, IS_NOT_BREAK_COMP) \
    if constexpr (sizeof...(rest) > 0) { \
        bool some_not_continue = ((flow_control = lambda.template operator()<rest>(), static_cast<bool>(IS_NOT_CONTINUE_COMP)) || ...); \
        if (some_not_continue && (IS_NOT_BREAK_COMP)) { \
            FLOW_RETURN(); \
        } \
        FLOW_BREAK(); \
    }

    if constexpr (FlowControl::features.has_return) {
        using OutFlowControl = make_flow_control<{true, false, false, true}, typename FlowControl::data_t>::type;

        #define FLOW_CONTINUE() \
        if constexpr (sizeof...(rest) > 0) { \
            return controlled_for<FlowControl, rest...>(std::forward<F>(lambda)); \
        } else { \
            return OutFlowControl::none_(); \
        }

        #define FLOW_BREAK() \
        return OutFlowControl::none_();

        #define FLOW_RETURN() \
        if constexpr (std::is_same_v<void, typename FlowControl::data_t>) { \
            return OutFlowControl::return_(); \
        } else { \
            return OutFlowControl::return_(std::move(flow_control).data()); \
        }

        if constexpr (
            FlowControl::tagging_strategy == detail::TAGGING_STRATEGY::INVARIANT_ONLY
        || FlowControl::tagging_strategy == detail::TAGGING_STRATEGY::INVARIANT_WITH_BOOL
        ) {
            if constexpr (FlowControl::is_convertible_to_integral) {
                switch (flow_control.id()) {
                    case FlowControl::continue_().id():
                        if constexpr (prefer_folding) {
                            FOLD_REST(flow_control.id() != FlowControl::continue_().id(), flow_control.id() != FlowControl::break_().id());
                        } else {
                            FLOW_CONTINUE();
                        }
                    case FlowControl::break_().id():
                        FLOW_BREAK();
                    default:
                        FLOW_RETURN();
                }
            } else {
                if (flow_control == FlowControl::continue_()) {
                    if constexpr (allow_folding) {
                        FOLD_REST(flow_control != FlowControl::continue_(), flow_control != FlowControl::break_());
                    } else {
                        FLOW_CONTINUE();
                    }
                } else if (flow_control == FlowControl::break_()) {
                    FLOW_BREAK();
                } else {
                    FLOW_RETURN();
                }
            }
        } else if constexpr (FlowControl::tagging_strategy == detail::TAGGING_STRATEGY::ENUM) {
            switch (flow_control.kind()) {
                case FlowControl::kind_t::CONTINUE:
                    if constexpr (prefer_folding) {
                        FOLD_REST(flow_control.kind() != FlowControl::kind_t::CONTINUE, flow_control.kind() != FlowControl::kind_t::BREAK);
                    } else {
                        FLOW_CONTINUE();
                    }
                case FlowControl::kind_t::BREAK:
                    FLOW_BREAK();
                default:
                    FLOW_RETURN();
            }
        } else {
            static_assert(false);
        }

        #undef FLOW_CONTINUE
        #undef FLOW_BREAK
        #undef FLOW_RETURN
    } else {
        #define FLOW_CONTINUE() \
        if constexpr (sizeof...(rest) > 0) { \
            return controlled_for<FlowControl, rest...>(std::forward<F>(lambda)); \
        } else { \
            return; \
        }

        #define FLOW_BREAK() \
        return;

        #define FLOW_RETURN() \
        std::unreachable();

        if constexpr (
            FlowControl::tagging_strategy == detail::TAGGING_STRATEGY::INVARIANT_ONLY
        || FlowControl::tagging_strategy == detail::TAGGING_STRATEGY::INVARIANT_WITH_BOOL
        ) {
            if constexpr (FlowControl::is_convertible_to_integral) {
                switch (flow_control.id()) {
                    case FlowControl::continue_().id():
                        if constexpr (prefer_folding) {
                            FOLD_REST(flow_control.id() != FlowControl::continue_().id(), flow_control.id() != FlowControl::break_().id());
                        } else {
                            FLOW_CONTINUE();
                        }
                    case FlowControl::break_().id():
                        FLOW_BREAK();
                    default:
                        FLOW_RETURN();
                }
            } else {
                if constexpr (allow_folding) {
                    FOLD_REST(flow_control != FlowControl::continue_(), flow_control != FlowControl::break_());
                } else {
                    FLOW_CONTINUE();
                }
            }
        } else if constexpr (FlowControl::tagging_strategy == detail::TAGGING_STRATEGY::ENUM) {
            switch (flow_control.kind()) {
                case FlowControl::kind_t::CONTINUE:
                    if constexpr (prefer_folding) {
                        FOLD_REST(flow_control.kind() != FlowControl::kind_t::CONTINUE, flow_control.kind() != FlowControl::kind_t::BREAK);
                    } else {
                        FLOW_CONTINUE();
                    }
                case FlowControl::kind_t::BREAK:
                    FLOW_BREAK();
                default:
                    FLOW_RETURN();
            }
        } else {
            static_assert(false);
        }

        #undef FLOW_CONTINUE
        #undef FLOW_BREAK
        #undef FLOW_RETURN
    }

    #undef FOLD_REST
}

} // namespace flow_control
