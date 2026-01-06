#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <cstring>
#include <type_traits>
#include <utility>

#include "./util/string_literal.hpp"
#include "./container/memory.hpp"
#include "./util/stringify.hpp"

namespace codegen {

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


struct CodeData {
    Buffer buffer;
    uint8_t indent;

    constexpr CodeData (Buffer&& buffer, uint8_t indent) : buffer(std::move(buffer)), indent(indent) {}

    [[nodiscard]] static constexpr char* make_indent (uint16_t indent_size, char* dst) {
        char* indent_end = dst + indent_size;
        for (; dst < indent_end; dst++) {
            *dst = ' ';
        }
        return dst;
    }

    template <typename ...T>
    constexpr void write_strs (T&&... strs) {
        uint16_t indent_size = indent * 4;
        size_t size = (... + stringify::get_str_size(strs)) + indent_size;
        Buffer::Index<char> dst_idx = buffer.next_multi_byte<char>(size);
        char *dst = buffer.get(dst_idx);
        dst = make_indent(indent_size, dst);
        ((
            dst = stringify::_write_string(dst, std::forward<T>(strs), buffer)
        ), ...);
    }
    template <typename ...T>
    constexpr void _line (T&&... strs) {
        write_strs(std::forward<T>(strs)..., "\n");
    }

    template <typename T>
    [[nodiscard, gnu::always_inline]] constexpr T& as () & {
        static_assert(std::is_base_of_v<CodeData, T>);
        static_assert(std::is_layout_compatible_v<CodeData, T>);
        return reinterpret_cast<T&>(*this);
    }

    template <typename T>
    [[nodiscard, gnu::always_inline]] constexpr T as () && {
        static_assert(std::is_base_of_v<CodeData, T>);
        static_assert(std::is_layout_compatible_v<CodeData, T>);
        return std::move(reinterpret_cast<T&>(*this));
    }
};

struct ClosedCodeBlock {
// private:
    Buffer buffer;

public:
    constexpr explicit ClosedCodeBlock (Buffer&& buffer) : buffer(std::move(buffer)) {}

    [[nodiscard]] constexpr const char* const& data () const {
        return reinterpret_cast<const char *const &>(buffer.data());
    }

    [[nodiscard]] constexpr const Buffer::index_t& size () const { return buffer.current_position(); }
};

template <typename Last, typename Derived>
struct CodeBlockBase : CodeData {
    using Last_ = Last;
    using Derived_ = Derived;

    // constexpr explicit CodeBlockBase (CodeData&& cd) : CodeData{std::move(cd)} {};

    template <typename T>
    constexpr auto _if (T&& condition) &&;

    template <typename T>
    constexpr auto _switch (T&& key) &&;

    template <typename ...T>
    constexpr auto _struct (T&&... strs) &&;

    template <typename ...T>
    constexpr Derived line (this auto&& self, T&&... strs) {
        self._line(std::forward<T>(strs)...);
        return std::forward<Derived>(self);
    }

    constexpr auto end () && {
        if constexpr (std::is_same_v<Last, ClosedCodeBlock>) {
            return ClosedCodeBlock{std::move(buffer)};
        } else {
            _end();
            return std::move(as<Last>());
        }
    }

protected:
    constexpr void _end () {
        indent--;
        _line("}");
    }
};

template <typename Last>
struct CodeBlock : CodeBlockBase<Last, CodeBlock<Last>> {
    // /*tag1*/ using CodeBlockBase<Last, CodeBlock<Last>>::CodeBlockBase;
};


template <typename Last>
struct If : CodeBlockBase<Last, If<Last>> {
    // /*tag1*/ using CodeBlockBase<Last, If<Last>>::CodeBlockBase;

    constexpr CodeBlock<Last> _else () && {
        this->indent--;
        this->_line("} else {");
        return std::move(this->template as<CodeBlock<Last>>());
    }
};
template <typename Last, typename Derived>
template <typename T>
constexpr auto CodeBlockBase<Last, Derived>::_if (T&& condition) && {
    _line("if (", std::forward<T>(condition), ") {");
    indent++;
    return std::move(this->template as<If<Derived>>());
};



template <typename Last>
struct Case : CodeBlockBase<Last, Case<Last>> {
    // /*tag1*/ using CodeBlockBase<Last, Case<Last>>::CodeBlockBase;

    constexpr auto end () &&;
};

template <typename Last>
struct Switch : CodeData {
    template <typename T>
    constexpr Case<Last> _case(T&& value) && {
        _line("case ", std::forward<T>(value), ": {");
        indent++;
        return std::move(as<Case<Last>>());
    };

    constexpr Case<Last> _default () && {
        _line("default: {");
        indent++;
        return std::move(as<Case<Last>>());
    }

    constexpr Last end () && {
        indent--;
        _line("}");
        return std::move(as<Last>());
    }
};
template <typename Last>
constexpr auto Case<Last>::end () && {
    this->_end();
    return std::move(this->template as<Switch<Last>>());
}
template <typename Last, typename Derived>
template <typename T>
constexpr auto CodeBlockBase<Last, Derived>::_switch (T&& key) && {
    _line("switch (", std::forward<T>(key), ") {");
    indent++;
    return std::move(as<Switch<Derived>>());
};

template <typename Last>
struct Method : CodeBlockBase<Last, Method<Last>> {
    // /*tag1*/ using CodeBlockBase<Last, Method<Last>>::CodeBlockBase;

    constexpr auto end () &&;
};

template <typename Last>
struct EmptyCtor : CodeBlockBase<Last, EmptyCtor<Last>> {
    // /*tag1*/ using CodeBlockBase<Last, EmptyCtor<Last>>::CodeBlockBase;

    constexpr auto end () &&;
};


template <typename Last, typename Derived, typename DerivedSimple = Derived>
struct StructBase : CodeData {
    using Last_ = Last;
    using Derived_ = Derived;

    // constexpr explicit StructBase (CodeData&& cd, Buffer::View<char> name = {}) : CodeData{std::move(cd)}, name(name) {};

    constexpr Derived _private (this auto&& self) {
        self._line("private:");
        return std::forward<Derived>(self);
    }
    constexpr Derived _public (this auto&& self) {
        self._line("public:");
        return std::forward<Derived>(self);
    }
    constexpr Derived _protected (this auto&& self) {
        self._line("protected:");
        return std::forward<Derived>(self);
    }

    template <typename T, typename U>
    constexpr Method<DerivedSimple> method (T&& type, U&& name) && {
        _line(std::forward<T>(type), " ", std::forward<U>(name), " () {");
        indent++;
        return std::move(as<Method<DerivedSimple>>());
    }

private:
    template <typename T, typename U, typename V>
    constexpr Method<DerivedSimple> method_0 (T&& type, U&& name, V&& args) && {
        _line(std::forward<T>(type), " ", std::forward<U>(name), " (", std::forward<V>(args), ") {");
        indent++;
        return std::move(as<Method<DerivedSimple>>());
    }

    template <typename T, typename U, typename V>
    constexpr Method<DerivedSimple> method_1 (T&& attributes, U&& type, V&& name) && {
        _line(std::forward<T>(attributes), " ", std::forward<U>(type), " ", std::forward<V>(name), " () {");
        indent++;
        return std::move(as<Method<DerivedSimple>>());
    }

    template <typename T, typename U, typename V, typename W>
    constexpr Method<DerivedSimple> method_2 (T&& attributes, U&& type, V&& name, W&& args) && {
        _line(std::forward<T>(attributes), " ", std::forward<U>(type), " ", std::forward<V>(name), " (", std::forward<W>(args), ") {");
        indent++;
        return std::move(as<Method<DerivedSimple>>());
    }

public:
    template <typename T, typename U, typename ...V>
    requires (sizeof...(V) > 0)
    constexpr Method<DerivedSimple> method (T&& type, U&& name, Args<V...>&& args) && {
        return std::move(*this).method_0(std::forward<T>(type), std::forward<U>(name), std::move(args));
    }

    template <typename T, typename U, typename ...V>
    requires (sizeof...(V) > 0)
    constexpr Method<DerivedSimple> method (T&& type, U&& name, const Args<V...>& args) && {
        return std::move(*this).method_0(std::forward<T>(type), std::forward<U>(name), args);
    }

    template <typename ...T, typename U, typename V>
    requires (sizeof...(T) > 0)
    constexpr Method<DerivedSimple> method (Attributes<T...>&& attributes, U&& type, V&& name) && {
        return std::move(*this).method_1(std::move(attributes), std::forward<U>(type), std::forward<V>(name));
    }

    template <typename ...T, typename U, typename V>
    requires (sizeof...(T) > 0)
    constexpr Method<DerivedSimple> method (const Attributes<T...>& attributes, U&& type, V&& name) && {
        return std::move(*this).method_1(attributes, std::forward<U>(type), std::forward<V>(name));
    }

    template <typename ...T, typename U, typename V, typename ...W>
    requires (sizeof...(T) > 0 && sizeof...(W) > 0)
    constexpr Method<DerivedSimple> method (Attributes<T...>&& attributes, U&& type, V&& name, Args<W...>&& args) && {
        return std::move(*this).method_2(std::move(attributes), std::forward<U>(type), std::forward<V>(name), std::move(args));
    }

    template <typename ...T, typename U, typename V, typename ...W>
    requires (sizeof...(T) > 0 && sizeof...(W) > 0)
    constexpr Method<DerivedSimple> method (const Attributes<T...>& attributes, U&& type, V&& name, Args<W...>&& args) && {
        return std::move(*this).method_2(attributes, std::forward<U>(type), std::forward<V>(name), std::move(args));
    }

    template <typename ...T, typename U, typename V, typename ...W>
    requires (sizeof...(T) > 0 && sizeof...(W) > 0)
    constexpr Method<DerivedSimple> method (Attributes<T...>&& attributes, U&& type, V&& name, const Args<W...>& args) && {
        return std::move(*this).method_2(std::move(attributes), std::forward<U>(type), std::forward<V>(name), args);
    }

    template <typename ...T, typename U, typename V, typename ...W>
    requires (sizeof...(T) > 0 && sizeof...(W) > 0)
    constexpr Method<DerivedSimple> method (const Attributes<T...>& attributes, U&& type, V&& name, const Args<W...>& args) && {
        return std::move(*this).method_2(attributes, std::forward<U>(type), std::forward<V>(name), args);
    }

    template <typename T, typename U>
    constexpr Derived field (this auto&& self, T&& type, U&& name) {
        self._line(std::forward<T>(type), " ", std::forward<U>(name), ";");
        return std::forward<Derived>(self);
    } 

    template <typename ...T>
    constexpr auto _struct (T&&... strs) &&;
};
template <typename Last>
constexpr auto Method<Last>::end () && {
    this->_end();
    return std::move(this->template as<Last>());
}

template <typename Last>
constexpr auto EmptyCtor<Last>::end () && {
    return std::move(this->template as<Last>());
}

template <typename Last>
struct Struct : StructBase<Last, Struct<Last>> {
    // /*tag1*/ using StructBase<Last, Struct<Last>>::StructBase;

    constexpr Last end () && {
        this->indent--;
        this->_line("};");
        return std::move(this->template as<Last>());
    }
};

template <typename Last, typename Derived, typename Base>
struct StructWithNameBase : Base {
    Buffer::View<char> name {Buffer::Index<char>{0}, 0};

    template <typename T, typename U>
    constexpr EmptyCtor<Base> ctor (T&& args, U&& initializers) && {
        this->_line(std::string_view{name.begin(this->buffer), name.length}, " (", std::forward<T>(args), ") : ", std::forward<U>(initializers), " {}");
        return std::move(this->template as<EmptyCtor<Base>>());
    }

    constexpr Base strip_name () && {
        return std::move(this->template as<Base>());
    }
};

template <typename Last>
struct StructWithName : StructWithNameBase<Last, StructWithName<Last>, Struct<Last>> {
    // /*tag1*/ using StructBase<Last, Struct<Last>>::StructBase;
};

template <typename Last, typename Derived>
template <typename ...T>
constexpr auto CodeBlockBase<Last, Derived>::_struct (T&&... strs) && {
    Buffer::Index<char> name_start_idx = buffer.position_idx<char>().add((indent * 4) + "struct "_sl.size());
    _line("struct ", std::forward<T>(strs)..., " {");
    Buffer::Index<char> name_end_idx = buffer.position_idx<char>().sub(" {"_sl.size());
    indent++;
    return StructWithName<Derived>{{std::move(as<Struct<Derived>>()), {name_start_idx, name_end_idx}}};
};

template <typename Last>
struct NestedStruct : StructBase<Last, NestedStruct<Last>> {
    // /*tag1*/ using StructBase<Last, NestedStruct<Last>>::StructBase;

    constexpr Last end () && {
        this->indent--;
        this->_line("};");
        return std::move(this->template as<Last>());
    }

    template<typename T>
    constexpr Last end (T&& name) && {
        this->indent--;
        this->_line("}; ", std::forward<T>(name), ";");
        return std::move(this->template as<Last>());
    }
};

template <typename Last>
struct NestedStructWithName : StructWithNameBase<Last, NestedStructWithName<Last>, NestedStruct<Last>> {
    // /*tag1*/ using StructBase<Last, NestedStruct<Last>>::StructBase;
};


template <typename Last, typename Derived, typename DerivedSimple>
template <typename ...T>
constexpr auto StructBase<Last, Derived, DerivedSimple>::_struct (T&&... strs) && {
    Buffer::Index<char> name_start_idx = buffer.position_idx<char>().add(7 + (indent * 4));
    _line("struct ", std::forward<T>(strs)..., " {");
    Buffer::Index<char> name_end_idx = buffer.position_idx<char>().sub(3);
    indent++;
    return NestedStructWithName<Derived>{{std::move(as<NestedStruct<Derived>>()), {name_start_idx, name_end_idx}}};
}

struct Empty {};
struct UnknownCodeBlock : CodeBlock<Empty> {};
struct UnknownStructBase : StructBase<Empty, UnknownStructBase> {
    // /*tag1*/ using StructBase<Empty, UnknownStructBase>::StructBase;
};
using UnknownStruct = Struct<Empty>;
using UnknownNestedStruct = NestedStruct<Empty>;

constexpr CodeBlock<ClosedCodeBlock> create_code (Buffer&& buffer) {
    return {CodeData{std::move(buffer), 0}};
}

#undef CODE_DATA_CTOR_ARGS

}