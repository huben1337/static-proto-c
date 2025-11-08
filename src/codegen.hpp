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
    constexpr CodeData (Buffer&& buffer, uint8_t indent) : buffer(std::move(buffer)), indent(indent) {}

    CodeData (const CodeData&) noexcept = delete;
    CodeData& operator = (const CodeData&) = delete;

    constexpr CodeData (CodeData&&) noexcept = default;
    constexpr CodeData& operator = (CodeData&&) = default;

    protected:
    Buffer buffer;
    uint8_t indent;

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
};

#define CODE_DATA_CTOR_ARGS CodeData{std::move(this->buffer), this->indent}

struct ClosedCodeBlock {
    private:
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


    constexpr auto _if (std::string_view condition);
    constexpr auto _switch (std::string_view key);
    template <typename ...T>
    constexpr auto _struct (T&&... strs);

    template <typename ...T>
    constexpr Derived line (T&&... strs) {
        _line(std::forward<T>(strs)...);
        return {{CODE_DATA_CTOR_ARGS}};
    }

    constexpr auto end () {
        if constexpr (std::is_same_v<Last, ClosedCodeBlock>) {
            return ClosedCodeBlock{std::move(buffer)};
        } else {
            _end();
            return Last{{CODE_DATA_CTOR_ARGS}};
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

    constexpr CodeBlock<Last> _else () {
        this->indent--;
        this->_line("} else {");
        return {{CODE_DATA_CTOR_ARGS}};
    }
};
template <typename Last, typename Derived>
constexpr auto CodeBlockBase<Last, Derived>::_if (std::string_view condition) {
    _line("if (", condition, ") {");
    indent++;
    return If<Derived>{{CODE_DATA_CTOR_ARGS}};
};



template <typename Last>
struct Case : CodeBlockBase<Last, Case<Last>> {
    // /*tag1*/ using CodeBlockBase<Last, Case<Last>>::CodeBlockBase;

    constexpr auto end ();
};

template <typename Last>
struct Switch : CodeData {
    constexpr Case<Last> _case(std::string_view value) {
        _line("case ", value, ": {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    };

    constexpr Case<Last> _default () {
        _line("default: {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    }

    constexpr Last end () {
        indent--;
        _line("}");
        return {{CODE_DATA_CTOR_ARGS}};
    }
};
template <typename Last>
constexpr auto Case<Last>::end ()  {
    this->_end();
    return Switch<Last>{{CODE_DATA_CTOR_ARGS}};
}
template <typename Last, typename Derived>
constexpr auto CodeBlockBase<Last, Derived>::_switch (std::string_view key) {
    _line("switch (", key, ") {");
    indent++;
    return Switch<Derived>{{CODE_DATA_CTOR_ARGS}};
};

template <typename Last>
struct Method : CodeBlockBase<Last, Method<Last>> {
    // /*tag1*/ using CodeBlockBase<Last, Method<Last>>::CodeBlockBase;

    constexpr auto end ();
};

template <typename Last>
struct EmptyCtor : CodeBlockBase<Last, EmptyCtor<Last>> {
    // /*tag1*/ using CodeBlockBase<Last, EmptyCtor<Last>>::CodeBlockBase;

    constexpr auto end ();
};


template <typename Last, typename Derived>
struct StructBase : CodeData {
    using Last_ = Last;
    using Derived_ = Derived;

    // constexpr explicit StructBase (CodeData&& cd, Buffer::View<char> name = {}) : CodeData{std::move(cd)}, name(name) {};

    Buffer::View<char> name {Buffer::Index<char>{0}, 0};

    constexpr Derived _private () {
        _line("private:");
        return {{CODE_DATA_CTOR_ARGS}};
    }
    constexpr Derived _public () {
        _line("public:");
        return {{CODE_DATA_CTOR_ARGS}};
    }
    constexpr Derived _protected () {
        _line("protected:");
        return {{CODE_DATA_CTOR_ARGS}};
    }


    template <typename T, typename U>
    constexpr EmptyCtor<Derived> ctor (T&& args, U&& initializers) {
        _line(std::string_view{name.begin(buffer), name.length}, " (", std::forward<T>(args), ") : ", std::forward<U>(initializers), " {}");
        return {{CODE_DATA_CTOR_ARGS}};
    }

    template <typename T, typename U>
    constexpr Method<Derived> method (T&& type, U&& name) {
        _line(std::forward<T>(type), " ", std::forward<U>(name), " () {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    }

    template <typename T, typename U, typename ...V>
    requires (sizeof...(V) > 0)
    constexpr Method<Derived> method (T&& type, U&& name, const Args<V...>& args) {
        _line(std::forward<T>(type), " ", std::forward<U>(name), " (", args, ") {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    }

    template <typename ...T, typename U, typename V>
    requires (sizeof...(T) > 0)
    constexpr Method<Derived> method (const Attributes<T...>& attributes, U&& type, V&& name) {
        _line(attributes, " ", std::forward<U>(type), " ", std::forward<V>(name), " () {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    }

    template <typename ...T, typename U, typename V, typename ...W>
    requires (sizeof...(T) > 0 && sizeof...(W) > 0)
    constexpr Method<Derived> method (const Attributes<T...>& attributes, U&& type, V&& name, const Args<W...>& args) {
        _line(attributes, " ", std::forward<U>(type), " ", std::forward<V>(name), " (", args, ") {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    }

    template <typename T, typename U>
    constexpr Derived field (T&& type, U&& name) {
        _line(std::forward<T>(type), " ", std::forward<U>(name), ";");
        return {{CODE_DATA_CTOR_ARGS}};
    } 

    template <typename ...T>
    constexpr auto _struct (T&&... strs);
};
template <typename Last>
constexpr auto Method<Last>::end ()  {
    this->_end();
    return Last{{CODE_DATA_CTOR_ARGS}};
}

template <typename Last>
constexpr auto EmptyCtor<Last>::end ()  {
    return Last{{CODE_DATA_CTOR_ARGS}};
}

template <typename Last>
struct Struct : StructBase<Last, Struct<Last>> {
    // /*tag1*/ using StructBase<Last, Struct<Last>>::StructBase;

    constexpr Last end () {
        this->indent--;
        this->_line("};");
        return {{CODE_DATA_CTOR_ARGS}};
    }
};
template <typename Last, typename Derived>
template <typename ...T>
constexpr auto CodeBlockBase<Last, Derived>::_struct (T&&... strs) {
    Buffer::Index<char> name_start_idx = buffer.position_idx<char>().add((indent * 4) + "struct "_sl.size());
    _line("struct ", std::forward<T>(strs)..., " {");
    Buffer::Index<char> name_end_idx = buffer.position_idx<char>().sub(" {"_sl.size());
    indent++;
    return Struct<Derived>{CODE_DATA_CTOR_ARGS, {name_start_idx, name_end_idx}};
};
template <typename Last>
struct NestedStruct : StructBase<Last, NestedStruct<Last>> {
    // /*tag1*/ using StructBase<Last, NestedStruct<Last>>::StructBase;

    constexpr Last end () {
        this->indent--;
        this->_line("};");
        return {{CODE_DATA_CTOR_ARGS}};
    }

    constexpr Last end (std::string_view name) {
        this->indent--;
        this->_line("}; ", name, ";");
        return {{CODE_DATA_CTOR_ARGS}};
    }
};
template <typename Last, typename Derived>
template <typename ...T>
constexpr auto StructBase<Last, Derived>::_struct (T&&... strs) {
    Buffer::Index<char> name_start_idx = buffer.position_idx<char>().add(7 + (indent * 4));
    _line("struct ", std::forward<T>(strs)..., " {");
    Buffer::Index<char> name_end_idx = buffer.position_idx<char>().sub(3);
    indent++;
    return NestedStruct<Derived>{CODE_DATA_CTOR_ARGS, {name_start_idx, name_end_idx}};
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