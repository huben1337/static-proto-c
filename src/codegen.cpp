#pragma once

#include "base.cpp"
#include <cstdint>
#include <string>
#include <cstring>
#include <cmath>
#include "string_helpers.cpp"
#include "string_literal.cpp"
#include "memory.cpp"

namespace codegen {

INLINE char* make_indent (uint16_t indent_size, char* dst) {
    char* indent_end = dst + indent_size;
    for (; dst < indent_end; dst++) {
        *dst = ' ';
    }
    return dst;
}

template <size_t N, size_t... Indices>
INLINE char* _write_c_sl (char* dst, const char (&value)[N], std::index_sequence<Indices...>) {
    ((dst[Indices] = value[Indices]), ...);
    return dst;
}
template <size_t N>
INLINE char* write_string(char* dst, const char (&value)[N]) {
    _write_c_sl(dst, value, std::make_index_sequence<N>{});
    return dst + N - 1;
}
template <size_t N>
INLINE size_t get_str_size (const char (&)[N]) {
    return N - 1;
}

template <size_t N, size_t... Indices>
INLINE char* _write_sl(char* dst, StringLiteral<N> value, std::index_sequence<Indices...>) {
    ((dst[Indices] = value.value[Indices]), ...);
    return dst;
}
template <size_t N>
INLINE char* write_string(char* dst, StringLiteral<N> value) {
    _write_sl(dst, value, std::make_index_sequence<N>{});
    return dst + N - 1;
}
template <size_t N>
INLINE size_t get_str_size (StringLiteral<N>) {
    return N - 1;
}

INLINE char* write_string (char* dst, std::string_view value) {
    size_t size = value.size();
    std::memcpy(dst, value.data(), size);
    return dst + size;
}
constexpr INLINE size_t get_str_size (std::string_view str) {
    return str.size();
}

constexpr INLINE size_t get_str_size (Buffer::Index<char>*) {
    return 0;
}


INLINE char* write_string (char* dst, size_t value) {
    if (value == 0) {
        *dst = '0';
        return dst + 1;
    } else {
        uint8_t length = std::log10(value) + 1;
        uint8_t i = length - 1;
        dst[i--] = '0' + (value % 10);
        value /= 10;
        while (value > 0) {
            dst[i--] = '0' + (value % 10);
            value /= 10;
        }
        return dst + length;
    }
}
INLINE size_t get_str_size (size_t value) {
    if (value == 0) {
        return 1;
    } else {
        return std::log10(value) + 1;
    }
}

template <typename ...T, size_t... Indices>
INLINE char* _write_str_tuple (char* dst, std::tuple<T...> values, std::index_sequence<Indices...>) {
    ((dst = write_string(dst, std::get<Indices>(values))), ...);
    return dst;
};
template <typename ...T>
INLINE char* write_string(char* dst, std::tuple<T...> values) {
    return _write_str_tuple(dst, values, std::make_index_sequence<sizeof...(T)>{});
}
template <typename ...T, size_t... Indices>
INLINE size_t _get_str_tuple_size (std::tuple<T...> values, std::index_sequence<Indices...>) {
    return (... + get_str_size(std::get<Indices>(values)));
};
template <typename ...T>
INLINE size_t get_str_size (std::tuple<T...> values) {
    return _get_str_tuple_size(values, std::make_index_sequence<sizeof...(T)>{});
}

template <typename T>
INLINE char* write_string_entry(char* dst, T&& value, char* dst_start, Buffer::Index<char> dst_idx) {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, Buffer::Index<char>*>) {
        uint32_t delta = dst - dst_start;
        *value = dst_idx.add(delta);
        return dst;
    } else {
        return write_string(dst, std::forward<T>(value));
    }
}


struct CodeData {
    CodeData (Buffer buffer, uint8_t indent) : buffer(buffer), indent(indent) {}
    protected:
    Buffer buffer;
    uint8_t indent;

    template <typename ...T>
    INLINE void write_strs (T&&... strs) {
        uint16_t indent_size = indent * 4;
        size_t size = (... + get_str_size(strs)) + indent_size;
        auto dst_idx = buffer.next_multi_byte<char>(size);
        char *dst = buffer.get(dst_idx);
        char* dst_start = dst;
        dst = make_indent(indent_size, dst);
        ((
            dst = write_string_entry(dst, strs, dst_start, dst_idx)
        ), ...);
    }
    template <typename ...T>
    INLINE void _line (T&&... strs) {
        write_strs(strs..., "\n");
    }
};

template <StringLiteral delimiter, typename ...T, size_t ...Indices>
INLINE char* _write_str_tuple (char* dst, std::tuple<T...> strs, std::index_sequence<Indices...>) {
    ((
        dst = write_string(write_string(dst, std::get<Indices>(strs)), delimiter)
    ), ...);
    return dst;
}
template <StringLiteral delimiter, typename ...T>
INLINE char* write_str_tuple (char* dst, std::tuple<T...> strs) {
    if constexpr (sizeof...(T) == 0) {
        return dst;
    } else {
        dst = _write_str_tuple<delimiter>(dst, strs, std::make_index_sequence<sizeof...(T) - 1>{});
        dst = write_string(dst, std::get<sizeof...(T) - 1>(strs));
        return dst;
    }
}

template <typename ...O, size_t ...Indcies>
INLINE size_t get_str_tuple_size (std::tuple<O...> strs, std::index_sequence<Indcies...>) {
    if constexpr (sizeof...(O) == 0) {
        return 0;
    } else {
        return (... + get_str_size(std::get<Indcies>(strs)) );
    }
};

struct ClosedCodeBlock {
    private:
    Buffer _buffer;
    public:
    ClosedCodeBlock (Buffer buffer) : _buffer(buffer) {}
    INLINE const char* c_str () {
        *_buffer.get_next<char>() = 0;
        return reinterpret_cast<const char*>(_buffer.c_memory());
    }
    INLINE const Buffer::index_t size () { return _buffer.current_position(); }
    INLINE Buffer buffer () { return _buffer; }    
};

template <typename Last, typename Derived>
struct __CodeBlock : CodeData {
    using __Last = Last;
    using __Derived = Derived;

    INLINE auto _if (std::string_view condition);
    INLINE auto _switch (std::string_view key);
    template <typename ...T>
    INLINE auto _struct (T&&... strs);

    template <typename ...T>
    INLINE Derived line (T&&... strs) {
        _line(strs...);
        return c_cast<Derived>(__CodeBlock<Last, Derived>({buffer, indent}));
    }

    INLINE Last end () {
        if constexpr (std::is_same_v<Last, ClosedCodeBlock>) {
            return ClosedCodeBlock(buffer);
        } else {
            _end();
            return c_cast<Last>(__CodeBlock<typename Last::__Last, typename Last::__Derived>({buffer, indent}));
        }
    }

    protected:
    INLINE void _end () {
        indent--;
        _line("}");
    }
};

template <typename Last>
struct CodeBlock : __CodeBlock<Last, CodeBlock<Last>> {};


template <typename Last>
struct If : __CodeBlock<Last, If<Last>> {
    INLINE auto _else () {
        this->indent--;
        this->_line("} else {");
        return CodeBlock<Last>({{this->buffer, this->indent}});
    }
};
template <typename Last, typename Derived>
INLINE auto __CodeBlock<Last, Derived>::_if (std::string_view condition) {
    _line("if (", condition, ") {");
    indent++;
    return If<Derived>({{buffer, indent}});
};



template <typename Last>
struct Case : __CodeBlock<Last, Case<Last>> {
    INLINE auto end ();
};

template <typename Last>
struct Switch : CodeData {
    INLINE auto _case(std::string_view value) {
        _line("case ", value, ": {");
        indent++;
        return Case<Last>({{buffer, indent}});
    };

    INLINE auto _default () {
        _line("default: {");
        indent++;
        return Case<Last>({{buffer, indent}});
    }

    INLINE auto end () {
        indent--;
        _line("}");
        return c_cast<Last>(__CodeBlock<typename Last::__Last, typename Last::__Derived>({buffer, indent}));
    }
};
template <typename Last>
INLINE auto Case<Last>::end ()  {
    this->_end();
    return Switch<Last>({this->buffer, this->indent});
}
template <typename Last, typename Derived>
INLINE auto __CodeBlock<Last, Derived>::_switch (std::string_view key) {
    _line("switch (", key, ") {");
    indent++;
    return Switch<Derived>({buffer, indent});
};

template <typename Last>
struct Method : __CodeBlock<Last, Method<Last>> {
    INLINE auto end ();
};

template <typename Last>
struct EmptyCtor : __CodeBlock<Last, EmptyCtor<Last>> {
    INLINE auto end ();
};


template <typename Last, typename Derived>
struct __Struct : CodeData {
    using __Last = Last;
    using __Derived = Derived;

    struct StructName {
        Buffer::Index<char> start;
        Buffer::Index<char> end;
    } name;

    INLINE auto _private () {
        _line("private:");
        return c_cast<Derived>(__Struct< Last, Derived>({buffer, indent}));
    }
    INLINE auto _public () {
        _line("public:");
        return c_cast<Derived>(__Struct<Last, Derived>({buffer, indent}));
    }
    INLINE auto _protected () {
        _line("protected:");
        return c_cast<Derived>(__Struct<Last, Derived>({buffer, indent}));
    }


    template <typename T, typename U>
    INLINE auto ctor (T&& args, U&& initializers) {
        _line(std::string_view{buffer.get(name.start), buffer.get(name.end)}, " (", args, ") : ", initializers, " {}");
        return EmptyCtor<Derived>({{buffer, indent}});
    }
    template <typename T, typename U>
    INLINE auto method (T&& type, U&& name) {
        uint16_t indent_size = indent * 4;
        size_t str_size = indent_size + get_str_size(type) + 1 + get_str_size(name) + 6;
        char* str = buffer.get(buffer.next_multi_byte<char>(str_size));
        str = make_indent(indent_size, str);
        str = write_string(str, type);
        str = write_string(str, " ");
        str = write_string(str, name);
        str = write_string(str, " () {\n");
        indent++;
        return Method<Derived>({{buffer, indent}});
    }

    template <typename T, typename U, typename ...V>
    requires (sizeof...(V) > 0)
    INLINE auto method (T&& type, U&& name, std::tuple<V...>&& args) {
        uint16_t indent_size = indent * 4;
        size_t str_size = indent_size + get_str_size(type) + 1 + get_str_size(name) + 2 + get_str_tuple_size(args, std::make_index_sequence<sizeof...(V)>{}) + (sizeof...(V) - 1)*2 + 4;
        char* str = buffer.get(buffer.next_multi_byte<char>(str_size));
        str = make_indent(indent_size, str);
        str = write_string(str, type);
        str = write_string(str, " ");
        str = write_string(str, name);
        str = write_string(str, " (");
        str = write_str_tuple<", ">(str, args);
        str = write_string(str, ") {\n");
        indent++;
        return Method<Derived>({{buffer, indent}});
    }

    template <typename ...T, typename U, typename V>
    requires (sizeof...(T) > 0)
    INLINE auto method (std::tuple<T...>&& attributes, U&& type, V&& name) {
        uint16_t indent_size = indent * 4;
        size_t str_size = indent_size + get_str_tuple_size(attributes, std::make_index_sequence<sizeof...(T)>{}) + (sizeof...(T) - 1) + 1 + get_str_size(type) + 1 + get_str_size(name) + 6;
        char* str = buffer.get(buffer.next_multi_byte<char>(str_size));
        str = make_indent(indent_size, str);
        str = write_str_tuple<" ">(str, attributes);
        str = write_string(str, " ");
        str = write_string(str, type);
        str = write_string(str, " ");
        str = write_string(str, name);
        str = write_string(str, " () {\n");
        indent++;
        return Method<Derived>({{buffer, indent}});
    }

    template <typename ...T, typename U, typename V, typename ...W>
    requires (sizeof...(T) > 0 && sizeof...(W) > 0)
    INLINE auto method (std::tuple<T...>&& attributes, U&& type, V&& name, std::tuple<W...>&& args) {
        uint16_t indent_size = indent * 4;
        size_t str_size = indent_size + get_str_tuple_size(attributes, std::make_index_sequence<sizeof...(T)>{}) + (sizeof...(T) - 1) + 1 + get_str_size(type) + 1 + get_str_size(name) + 2 + get_str_tuple_size(args, std::make_index_sequence<sizeof...(W)>{}) + (sizeof...(W) - 1)*2 + 4;
        char* str = buffer.get(buffer.next_multi_byte<char>(str_size));
        str = make_indent(indent_size, str);
        str = write_str_tuple<" ">(str, attributes);
        str = write_string(str, " ");
        str = write_string(str, type);
        str = write_string(str, " ");
        str = write_string(str, name);
        str = write_string(str, " (");
        str = write_str_tuple<", ">(str, args);
        str = write_string(str, ") {\n");
        indent++;
        return Method<Derived>({{buffer, indent}});
    }

    template <typename T, typename U>
    INLINE Derived field (T&& type, U&& name) {
        _line(type, " ", name, ";");
        return c_cast<Derived>(__Struct<Last, Derived>({buffer, indent}));
    } 

    template <typename ...T>
    INLINE auto _struct (T&&... strs);
};
template <typename Last>
INLINE auto Method<Last>::end ()  {
    this->_end();
    return c_cast<Last>(__Struct<typename Last::__Last, typename Last::__Derived>({this->buffer, this->indent}));
}

template <typename Last>
INLINE auto EmptyCtor<Last>::end ()  {
    return c_cast<Last>(__Struct<typename Last::__Last, typename Last::__Derived>({this->buffer, this->indent}));
}

template <typename Last>
struct Struct : __Struct<Last, Struct<Last>> {
    INLINE auto end () {
        this->indent--;
        this->_line("};");
        return c_cast<Last>(__CodeBlock<typename Last::__Last, typename Last::__Derived>({this->buffer, this->indent}));
    }
};
template <typename Last, typename Derived>
template <typename ...T>
INLINE auto __CodeBlock<Last, Derived>::_struct (T&&... strs) {
    Buffer::Index<char> name_start_idx = buffer.position_idx<char>().add(7 + indent * 4);
    _line("struct ", strs..., " {");
    Buffer::Index<char> name_end_idx = buffer.position_idx<char>().sub(3);
    indent++;
    return Struct<Derived>({{buffer, indent}, {name_start_idx, name_end_idx}});
};
template <typename Last>
struct NestedStruct : __Struct<Last, NestedStruct<Last>> {
    INLINE auto end () {
        this->indent--;
        this->_line("};");
        return _end();
    }

    INLINE auto end (std::string_view name) {
        this->indent--;
        this->_line("}; ", name, ";");
        return _end();
    }

    private:
    INLINE auto _end () {
        return c_cast<Last>(__Struct<typename Last::__Last, typename Last::__Derived>({this->buffer, this->indent}));
    }
};
template <typename Last, typename Derived>
template <typename ...T>
INLINE auto __Struct<Last, Derived>::_struct (T&&... strs) {
    Buffer::Index<char> name_start_idx = buffer.position_idx<char>().add(7 + indent * 4);
    _line("struct ", strs..., " {");
    Buffer::Index<char> name_end_idx = buffer.position_idx<char>().sub(3);
    indent++;
    return NestedStruct<Derived>({{buffer, indent}, {name_start_idx, name_end_idx}});
}

struct Empty {};
struct UnknownCodeBlock : CodeBlock<Empty> {};
struct __UnknownStruct : __Struct<Empty, __UnknownStruct> {};
typedef Struct<Empty> UnknownStruct;
typedef NestedStruct<Empty> UnknownNestedStruct;

template <size_t N>
INLINE auto create_code (uint8_t (&memory)[N]) {
    return CodeBlock<ClosedCodeBlock>({{Buffer(memory), 0}});
}

}