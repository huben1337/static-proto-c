#pragma once

#include <algorithm>
#include <cstdint>
#include <gsl/util>
#include <string_view>
#include <type_traits>
#include <utility>

#include "./util/string_literal.hpp"
#include "./util/stringify.hpp"
#include "./estd/utility.hpp"
#include "./estd/empty.hpp"
#include "./estd/vector32.hpp"
#include "estd/ranges.hpp"

namespace codegen {

// TODO Make struct store rvalue-refernec to CodeData instead of deriving from it

template <typename... T>
struct StringParts : stringify::Stringifyable<"", T...> {
    using stringify::Stringifyable<"", T...>::Stringifyable;
};
template <typename... T>
StringParts (T&&...) -> StringParts<T...>;

template <typename... T>
struct Attributes : stringify::Stringifyable<" ", T...> {
    using stringify::Stringifyable<" ", T...>::Stringifyable;
};
template <typename... T>
Attributes (T&&...) -> Attributes<T...>;

template <typename... T>
struct Args : stringify::Stringifyable<", ", T...> {
    using stringify::Stringifyable<", ", T...>::Stringifyable;
};
template <typename... T>
Args (T&&...) -> Args<T...>;


template <typename... T>
struct is_string_parts_t : std::false_type {};
template <typename... T>
struct is_string_parts_t<StringParts<T...>> : std::true_type {};

template <typename... T>
struct is_args_t : std::false_type {};
template <typename... T>
struct is_args_t<Args<T...>> : std::true_type {};

template <typename... T>
struct is_attributes_t : std::false_type {};
template <typename... T>
struct is_attributes_t<Attributes<T...>> : std::true_type {};


struct ClosedCodeBlock {
private:
    estd::vector32<char> buffer;

public:
    constexpr explicit ClosedCodeBlock (estd::vector32<char>&& buffer) : buffer(std::move(buffer)) {}

    [[nodiscard]] constexpr const char* data () const {
        return estd::trivial_ptr_cast<const char>(buffer.data());
    }

    [[nodiscard]] constexpr const uint32_t& size () const { return buffer.size(); }

    [[nodiscard]] constexpr estd::vector32<char>&& steal_buffer () && {
        return std::move(buffer);
    }
};

template <typename Last>
struct CodeBlock;

template <typename Last>
struct If;

template <typename Last>
struct Switch;

template <typename Last>
struct Case;

template <typename Last>
struct Method;

template <typename Last>
struct EmptyCtor;

template <typename Last>
struct Struct;

template <typename Last>
struct NestedStruct;

template <typename Last>
struct StructWithName;

template <typename Last>
struct NestedStructWithName;

constexpr CodeBlock<ClosedCodeBlock> create_code (estd::vector32<char>&& buffer);

namespace detail {
    struct CodeData {
        CodeData() = delete;
        template <typename, typename>
        friend struct CodeBlockBase;

        friend struct StructDefintionEligibleBase;

        template <typename, typename>
        friend struct BasicStructFunctionalityProvider;

        template <typename, typename>
        friend struct StructFunctionalityProvider;

        template <typename, typename>
        friend struct NestedStructFunctionalityProvider;

        template <typename, typename>
        friend struct StructBase;
        
        template <typename, typename, typename>
        friend struct StructWithNameBase;

        template <typename>
        friend struct codegen::CodeBlock;

        template <typename>
        friend struct codegen::If;

        template <typename>
        friend struct codegen::Switch;

        template <typename>
        friend struct codegen::Case;

        template <typename>
        friend struct codegen::Method;

        template <typename>
        friend struct codegen::EmptyCtor;

        friend constexpr CodeBlock<ClosedCodeBlock> codegen::create_code (estd::vector32<char>&& buffer);

    private:
        estd::vector32<char> buffer;
        uint8_t indent;

        constexpr CodeData (estd::vector32<char>&& buffer, uint8_t indent) : buffer(std::move(buffer)), indent(indent) {}

        struct IndentGenerator {
            uint16_t indent_size;

            [[nodiscard]] stringify::Dst&& write (stringify::Dst&& dst) const {
                dst.write_n(' ', indent_size);
                return std::move(dst);
            }

            [[nodiscard]] uint32_t get_size () const {
                return indent_size;
            }
        };

        template <typename ...T>
        constexpr void write_strs (T&&... strs) {
            const uint16_t indent_size = indent * 4;
            stringify::write_into(buffer, IndentGenerator{indent_size}, std::forward<T>(strs)...);
        }
    
        template <typename ...T>
        constexpr void _line (T&&... strs) {
            write_strs(std::forward<T>(strs)..., "\n");
        }

    public:
        template <typename T>
        [[nodiscard, gnu::always_inline]] constexpr const T& as (this const CodeData& self) {
            static_assert(std::is_base_of_v<CodeData, T>);
            static_assert(std::is_layout_compatible_v<CodeData, T>);
            return static_cast<const T&>(self);
        }

        template <typename T>
        [[nodiscard, gnu::always_inline]] constexpr T& as (this CodeData& self) {
            static_assert(std::is_base_of_v<CodeData, T>);
            static_assert(std::is_layout_compatible_v<CodeData, T>);
            return static_cast<T&>(self);
        }

        template <typename T>
        [[nodiscard, gnu::always_inline]] constexpr T&& as (this CodeData&& self) {
            static_assert(std::is_base_of_v<CodeData, T>);
            static_assert(std::is_layout_compatible_v<CodeData, T>);
            return static_cast<T&&>(self);
        }
    };

    struct StructDefintionEligibleBase {
        template <typename ...T>
        [[nodiscard]] static constexpr estd::integral_range<uint32_t> begin_struct(auto& self, T&&... strs) {
            const uint32_t name_start_idx = self.buffer.size() + (self.indent * 4) + "struct "_sl.size();
            self._line("struct ", std::forward<T>(strs)..., " {");
            const uint32_t name_end_idx = self.buffer.size() - " {"_sl.size() - 1;
            self.indent++;
            return {name_start_idx, name_end_idx};
        }
    };

    // struct CodeBlockBaseBaseBase : CodeData {
    //     using CodeData::CodeData;

    // protected:
    //     constexpr void _end (this CodeData& self) {
    //         self.indent--;
    //         self._line("}");
    //     }
    // };

    // template <typename Last>
    // struct CodeBlockBaseBase : CodeBlockBaseBaseBase {
    //     using CodeBlockBaseBaseBase::CodeBlockBaseBaseBase;

    //     constexpr Last&& end (this CodeBlockBaseBase&& self) {
    //         self._end();
    //         return std::move(self).template as<Last>();
    //     }
    // };

    // template <>
    // struct CodeBlockBaseBase<ClosedCodeBlock> : CodeBlockBaseBaseBase {
    //     using CodeBlockBaseBaseBase::CodeBlockBaseBaseBase;

    //     constexpr ClosedCodeBlock end (this CodeData&& self) {
    //         return ClosedCodeBlock{std::move(self.buffer)};
    //     }
    // };


    template <typename Derived, typename Last>
    struct CodeBlockBase : CodeData {    
        friend Derived;

    private:
        constexpr explicit CodeBlockBase(CodeData&& data) : CodeData(std::move(data)) {}
        
    public:
        template <typename T>
        constexpr If<Derived>&& _if (this Derived&& self, T&& condition) {
            self._line("if (", std::forward<T>(condition), ") {");
            self.indent++;
            return std::move(self).template as<If<Derived>>();
        }

        template <typename T>
        constexpr Switch<Derived>&& _switch (this Derived&& self, T&& key) {
            self._line("switch (", std::forward<T>(key), ") {");
            self.indent++;
            return std::move(self).template as<Switch<Derived>>();
        }

        template <typename ...T>
        constexpr StructWithName<Derived> _struct (this Derived&& self, T&&... strs) {
            auto name = StructDefintionEligibleBase::begin_struct(self, std::forward<T>(strs)...);
            return StructWithName<Derived>{{name, std::move(self)}};
        }

        template <typename ...T>
        constexpr Derived&& line (this Derived&& self, T&&... strs) {
            self._line(std::forward<T>(strs)...);
            return std::move(self);
        }

        constexpr decltype(auto) end (this Derived&& self) {
            if constexpr (std::is_same_v<Last, ClosedCodeBlock>) {
                return ClosedCodeBlock{std::move(self.buffer)};
            } else {
                self._end();
                return std::move(self).template as<Last>();
            }
        }

    protected:
        constexpr void _end (this CodeData& self) {
            self.indent--;
            self._line("}");
        }
    };

    template <typename Derived, typename DerivedSimple>
    struct BasicStructFunctionalityProvider {
        friend Derived;

        template<typename, typename>
        friend struct StructBase;

        template<typename, typename, typename>
        friend struct StructWithNameBase;

    private:
        constexpr BasicStructFunctionalityProvider() = default;

    public:
        constexpr Derived&& _private (this Derived&& self) {
            self.data()._line("private:");
            return std::move(self);
        }
        constexpr Derived&& _public (this Derived&& self) {
            self.data()._line("public:");
            return std::move(self);
        }
        constexpr Derived&& _protected (this Derived&& self) {
            self.data()._line("protected:");
            return std::move(self);
        }

        using method_t = Method<DerivedSimple>;

        template <typename T, typename U>
        constexpr method_t&& method (this Derived&& self, T&& type, U&& name) {
            self.data()._line(std::forward<T>(type), " ", std::forward<U>(name), " () {");
            self.data().indent++;
            return std::move(self).data().template as<method_t>();
        }

    private:
        template <typename T, typename U, typename V>
        constexpr method_t&& method_0 (this Derived&& self, T&& type, U&& name, V&& args) {
            self.data()._line(std::forward<T>(type), " ", std::forward<U>(name), " (", std::forward<V>(args), ") {");
            self.data().indent++;
            return std::move(self).data().template as<method_t>();
        }

        template <typename T, typename U, typename V>
        constexpr method_t&& method_1 (this Derived&& self, T&& attributes, U&& type, V&& name) {
            self.data()._line(std::forward<T>(attributes), " ", std::forward<U>(type), " ", std::forward<V>(name), " () {");
            self.data().indent++;
            return std::move(self).data().template as<method_t>();
        }

        template <typename T, typename U, typename V, typename W>
        constexpr method_t&& method_2 (this Derived&& self, T&& attributes, U&& type, V&& name, W&& args) {
            self.data()._line(std::forward<T>(attributes), " ", std::forward<U>(type), " ", std::forward<V>(name), " (", std::forward<W>(args), ") {");
            self.data().indent++;
            return std::move(self).data().template as<method_t>();
        }

    public:
        template <typename T, typename U, typename ...V>
        requires (sizeof...(V) > 0)
        constexpr method_t&& method (this Derived&& self, T&& type, U&& name, Args<V...>&& args) {
            return std::move(self).method_0(std::forward<T>(type), std::forward<U>(name), std::move(args));
        }

        template <typename T, typename U, typename ...V>
        requires (sizeof...(V) > 0)
        constexpr method_t&& method (this Derived&& self, T&& type, U&& name, const Args<V...>& args) {
            return std::move(self).method_0(std::forward<T>(type), std::forward<U>(name), args);
        }

        template <typename ...T, typename U, typename V>
        requires (sizeof...(T) > 0)
        constexpr method_t&& method (this Derived&& self, Attributes<T...>&& attributes, U&& type, V&& name) {
            return std::move(self).method_1(std::move(attributes), std::forward<U>(type), std::forward<V>(name));
        }

        template <typename ...T, typename U, typename V>
        requires (sizeof...(T) > 0)
        constexpr method_t&& method (this Derived&& self, const Attributes<T...>& attributes, U&& type, V&& name) {
            return std::move(self).method_1(attributes, std::forward<U>(type), std::forward<V>(name));
        }

        template <typename ...T, typename U, typename V, typename ...W>
        requires (sizeof...(T) > 0 && sizeof...(W) > 0)
        constexpr method_t&& method (this Derived&& self, Attributes<T...>&& attributes, U&& type, V&& name, Args<W...>&& args) {
            return std::move(self).method_2(std::move(attributes), std::forward<U>(type), std::forward<V>(name), std::move(args));
        }

        template <typename ...T, typename U, typename V, typename ...W>
        requires (sizeof...(T) > 0 && sizeof...(W) > 0)
        constexpr method_t&& method (this Derived&& self, const Attributes<T...>& attributes, U&& type, V&& name, Args<W...>&& args) {
            return std::move(self).method_2(attributes, std::forward<U>(type), std::forward<V>(name), std::move(args));
        }

        template <typename ...T, typename U, typename V, typename ...W>
        requires (sizeof...(T) > 0 && sizeof...(W) > 0)
        constexpr method_t&& method (this Derived&& self, Attributes<T...>&& attributes, U&& type, V&& name, const Args<W...>& args) {
            return std::move(self).method_2(std::move(attributes), std::forward<U>(type), std::forward<V>(name), args);
        }

        template <typename ...T, typename U, typename V, typename ...W>
        requires (sizeof...(T) > 0 && sizeof...(W) > 0)
        constexpr method_t&& method (this Derived&& self, const Attributes<T...>& attributes, U&& type, V&& name, const Args<W...>& args) {
            return std::move(self).method_2(attributes, std::forward<U>(type), std::forward<V>(name), args);
        }

        template <typename T, typename U>
        constexpr Derived&& field (this Derived&& self, T&& type, U&& name) {
            self.data()._line(std::forward<T>(type), " ", std::forward<U>(name), ";");
            return std::move(self);
        }

        template <typename ...T>
        constexpr NestedStructWithName<Derived> _struct (this Derived&& self, T&&... strs) {
            auto name = StructDefintionEligibleBase::begin_struct(self.data(), std::forward<T>(strs)...);
            return NestedStructWithName<Derived>{{name, std::move(self).data().template as<NestedStruct<Derived>>()}};
        }
    };

    template <typename Derived, typename Last>
    struct StructFunctionalityProvider {
        friend Derived;

        template <typename, typename>
        friend struct CodeBlockBase;

        template <typename, typename>
        friend struct BasicStructFunctionalityProvider;

        template <typename, typename>
        friend struct NestedStructFunctionalityProvider;

    private:
        constexpr StructFunctionalityProvider() = default;

    public:
        constexpr Last&& end (this Derived&& self) {
            self.data().indent--;
            self.data()._line("};");
            return std::move(self).data().template as<Last>();
        }
    };

    template <typename Derived, typename Last>
    struct NestedStructFunctionalityProvider : StructFunctionalityProvider<Derived, Last> {
        friend Derived;

        template <typename, typename>
        friend struct CodeBlockBase;

        template <typename, typename>
        friend struct BasicStructFunctionalityProvider;
    private:
        constexpr NestedStructFunctionalityProvider() = default;

    public:
        constexpr Last&& end (this Derived&& self) {
            self.data().indent--;
            self.data()._line("};");
            return std::move(self).data().template as<Last>();
        }
        
        template<typename T>
        constexpr Last&& end (this Derived&& self, T&& name) {
            self.data().indent--;
            self.data()._line("}; ", std::forward<T>(name), ";");
            return std::move(self).data().template as<Last>();
        }
    };

    template <typename Derived, typename Last>
    struct StructBase : CodeData, BasicStructFunctionalityProvider<Derived, Derived> {
        template <typename, typename>
        friend struct BasicStructFunctionalityProvider;
        
        template <typename, typename>
        friend struct StructFunctionalityProvider;

        template <typename, typename>
        friend struct NestedStructFunctionalityProvider;

        StructBase() = delete;
    private:

        CodeData& data(this StructBase& self) {
            return self;
        }

        CodeData&& data(this StructBase&& self) {
            return std::move(self);
        }
    };

    template <typename Derived, typename DerivedSimple, typename Last>
    struct StructWithNameBase : BasicStructFunctionalityProvider<Derived, DerivedSimple>
    {
        friend Derived;

        template <typename, typename>
        friend struct CodeBlockBase;

        template <typename, typename>
        friend struct BasicStructFunctionalityProvider;

        template <typename, typename>
        friend struct StructFunctionalityProvider;

        template <typename, typename>
        friend struct NestedStructFunctionalityProvider;        
    private:

        estd::integral_range<uint32_t> name_idx_range;
        CodeData&& _data;


        CodeData& data() & {
            return _data;
        }

        CodeData&& data() && {
            return std::move(_data);
        }


        constexpr StructWithNameBase(const estd::integral_range<uint32_t>& name, CodeData&& data)
            : name_idx_range(name),
            _data(std::move(data)) {}

    public:
        StructWithNameBase(const StructWithNameBase&) = delete;
        StructWithNameBase(StructWithNameBase&&) = delete;

        StructWithNameBase& operator = (const StructWithNameBase&) = delete;
        StructWithNameBase& operator = (StructWithNameBase&&) = delete;

        constexpr ~StructWithNameBase() = default;

        template <typename T, typename U>
        constexpr EmptyCtor<DerivedSimple>&& ctor (this StructWithNameBase&& self, T&& args, U&& initializers) {
            const std::string_view name {self.name_idx_range.access_subspan(self.data().buffer)};
            self.data()._line(name, " (", std::forward<T>(args), ") : ", std::forward<U>(initializers), " {}");
            return std::move(self).data().template as<EmptyCtor<DerivedSimple>>();
        }

        constexpr DerivedSimple&& strip_name (this StructWithNameBase&& self) {
            return std::move(self).data().template as<DerivedSimple>();
        }
    };
} // namespace detail

template <typename Last>
struct CodeBlock : detail::CodeBlockBase<CodeBlock<Last>, Last> {
    constexpr explicit CodeBlock(detail::CodeData&& data) :
        detail::CodeBlockBase<CodeBlock<Last>, Last>(std::move(data))
    {}
};


template <typename Last>
struct If : detail::CodeBlockBase<If<Last>, Last> {
    template<typename  T>
    constexpr If&& _if_else (this If&& self, T&& condition) {
        self.indent--;
        self._line("} else if (", std::forward<T>(condition), ") {");
        self.indent++;
        return std::move(self);
    }

    constexpr CodeBlock<Last>&& _else (this If&& self) {
        self.indent--;
        self._line("} else {");
        return std::move(self).template as<CodeBlock<Last>>();
    }
};

template <typename Last>
struct Switch : detail::CodeData {
    template <typename T>
    constexpr Case<Last>&& _case(this Switch&& self, T&& value) {
        self._line("case ", std::forward<T>(value), ": {");
        self.indent++;
        return std::move(self).template as<Case<Last>>();
    }

    constexpr Case<Last>&& _default (this Switch&& self) {
        self._line("default: {");
        self.indent++;
        return std::move(self).template as<Case<Last>>();
    }

    constexpr Last&& end (this Switch&& self) {
        self.indent--;
        self._line("}");
        return std::move(self).template as<Last>();
    }
};

template <typename Last>
struct Case : detail::CodeBlockBase<Case<Last>, Last> {
    constexpr Switch<Last>&& end (this Case&& self) {
        self._end();
        return std::move(self).template as<Switch<Last>>();
    }
};

template <typename Last>
struct Method : detail::CodeBlockBase<Method<Last>, Last> {
    constexpr Last&& end (this Method&& self) {
        self._end();
        return std::move(self).template as<Last>();
    }
};

template <typename Last>
struct EmptyCtor : detail::CodeBlockBase<EmptyCtor<Last>, Last> {
    constexpr Last&& end (this EmptyCtor&& self) {
        return std::move(self).template as<Last>();
    }
};

// template <typename Last>
// struct Struct : detail::BasicStructFunctionalityProvider<Last, Struct<Last>> {
//     constexpr Last&& end (this Struct&& self) {
//         self.indent--;
//         self._line("};");
//         return std::move(self).template as<Last>();
//     }
// };

// template <typename Last>
// struct NestedStruct : detail::BasicStructFunctionalityProvider<Last, NestedStruct<Last>> {
//     constexpr Last&& end (this NestedStruct&& self) {
//         self.indent--;
//         self._line("};");
//         return std::move(self).template as<Last>();
//     }

//     template<typename T>
//     constexpr Last&& end (this NestedStruct&& self, T&& name) {
//         self.indent--;
//         self._line("}; ", std::forward<T>(name), ";");
//         return std::move(self).template as<Last>();
//     }
// };

template <typename Last>
struct Struct :
    detail::StructBase<Struct<Last>, Last>,
    detail::StructFunctionalityProvider<Struct<Last>, Last>
{};

template <typename Last>
struct NestedStruct :
    detail::StructBase<NestedStruct<Last>, Last>,
    detail::NestedStructFunctionalityProvider<NestedStruct<Last>, Last>
{};

template <typename Last>
struct StructWithName :
    detail::StructWithNameBase<StructWithName<Last>,Struct<Last>, Last>,
    detail::StructFunctionalityProvider<StructWithName<Last>, Last>
{};

template <typename Last>
struct NestedStructWithName :
    detail::StructWithNameBase<NestedStructWithName<Last>, NestedStruct<Last>, Last>,
    detail::NestedStructFunctionalityProvider<NestedStructWithName<Last>, Last>
{};

struct UnknownCodeBlock : CodeBlock<estd::empty> {};

struct UnknownStructBase : detail::StructBase<UnknownStructBase, void> {};

struct UnknownStruct : Struct<estd::empty> {};

struct UnknownNestedStruct : NestedStruct<estd::empty> {};

using UnknownMethod = Method<UnknownStructBase>;

constexpr CodeBlock<ClosedCodeBlock> create_code (estd::vector32<char>&& buffer) {
    return CodeBlock<ClosedCodeBlock>{{std::move(buffer), 0}};
}

}