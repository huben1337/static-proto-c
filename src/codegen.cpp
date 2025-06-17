#pragma once

#include "base.cpp"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>
#include "helper_types.cpp"
#include "string_literal.cpp"
#include "memory.cpp"
#include "fast_math.cpp"

namespace codegen {

template <StringLiteral seperator, typename ...T>
struct _CodeParts {
    INLINE constexpr _CodeParts (T&&... args, ...) : values(std::forward<T>(args)...) {}
    const std::tuple<const T...> values;
};

template <typename... T>
struct StringParts : _CodeParts<"", T...> {
    using _CodeParts<"", T...>::_CodeParts;
};
template <typename... T>
StringParts (T&&...) -> StringParts<T...>;

template <typename... T>
struct Attributes : _CodeParts<" ", T...> {
    using _CodeParts<" ", T...>::_CodeParts;
};
template <typename... T>
Attributes (T&&...) -> Attributes<T...>;


template <typename... T>
struct Args : _CodeParts<", ", T...> {
    using _CodeParts<", ", T...>::_CodeParts;
};
template <typename... T>
Args (T&&...) -> Args<T...>;

// int x = 0;
// auto a = _CodeParts<"">("as_", 1, x);
// auto b = StringParts("as_", 1, x);

INLINE char* make_indent (uint16_t indent_size, char* dst) {
    char* indent_end = dst + indent_size;
    for (; dst < indent_end; dst++) {
        *dst = ' ';
    }
    return dst;
}

template <std::unsigned_integral SizeT>
struct GeneratorBase {
    virtual SizeT get_size() const = 0;
};

template <typename GeneratorT>
concept GeneratorBaseType = requires (GeneratorT generator) {
    { generator.get_size() } -> std::unsigned_integral;
};


template <std::unsigned_integral SizeT>
struct Generator : GeneratorBase<SizeT> {
    virtual char* write(char*) const = 0;
};

template <typename GeneratorT>
concept GeneratorType = GeneratorBaseType<GeneratorT> && requires(GeneratorT generator, char* dst) {
    { generator.write(dst) } -> std::same_as<char*>;
};

template <GeneratorType T>
INLINE char* write_string(char* dst, T&& generator) {
    return generator.write(dst);
}
template <GeneratorType T>
INLINE char* _write_string(char* dst, T&& generator, const Buffer&) {
    return write_string(dst, std::forward<T>(generator));
}

struct _OverAllocatedGenerator {
    struct WriteResult {
        char* dst;
        Buffer::index_t over_allocation;
    };
};

template <std::unsigned_integral SizeT>
struct OverAllocatedGenerator : GeneratorBase<SizeT> {
    using WriteResult = _OverAllocatedGenerator::WriteResult;
    virtual WriteResult write (char*) const = 0;
};

template <typename GeneratorT>
concept OverAllocatedGeneratorType = GeneratorBaseType<GeneratorT> && requires(GeneratorT generator, char* dst) {
    { generator.write(dst) } -> std::same_as<_OverAllocatedGenerator::WriteResult>;
};

template <OverAllocatedGeneratorType T>
INLINE char* _write_string (char* dst, T&& generator, Buffer& buffer) {
    _OverAllocatedGenerator::WriteResult result = generator.write(dst);
    buffer.go_back(result.over_allocation);
    return result.dst;
}


struct _UnderAllocatedGenerator {
    struct Allocator {
        Allocator (Buffer& buffer) : buffer(buffer) {}
        private:
        Buffer& buffer;
        public:
        void allocate (size_t size) const {
            buffer.next_multi_byte<char>(size);
        }
    };
};

template <std::unsigned_integral SizeT>
struct UnderAllocatedGenerator : GeneratorBase<SizeT> {
    using Allocator = _UnderAllocatedGenerator::Allocator;
    virtual char* write (char*, Allocator&&) const = 0;
};

template <typename GeneratorT>
concept UnderAllocatedGeneratorType = GeneratorBaseType<GeneratorT> && requires(GeneratorT generator, char* dst, _UnderAllocatedGenerator::Allocator&& allocator) {
    { generator.write(dst, allocator) } -> std::same_as<char*>;
};

template <UnderAllocatedGeneratorType T>
INLINE char* _write_string (char* dst, T&& generator, Buffer& buffer) {
    return generator.write(dst, _UnderAllocatedGenerator::Allocator(buffer));
}



template <GeneratorBaseType T>
INLINE size_t get_str_size (T&& generator) {
    return generator.get_size();
}



template <size_t N, size_t... Indices>
INLINE char* _write_c_sl (char* dst, const char (&value)[N], std::index_sequence<Indices...>) {
    ((dst[Indices] = value[Indices]), ...);
    return dst;
}
template <size_t N>
INLINE char* write_string (char* dst, const char (&value)[N]) {
    _write_c_sl(dst, value, std::make_index_sequence<N>{});
    return dst + N - 1;
}
template<size_t N>
INLINE char* _write_string (char* dst, const char (&value)[N], const Buffer&) {
    return write_string(dst, value);
}

template <size_t N>
INLINE size_t get_str_size (const char (&)[N]) {
    return N - 1;
}


template <size_t N, size_t... Indices>
INLINE char* _write_sl (char* dst, const StringLiteral<N>& value, std::index_sequence<Indices...>) {
    ((dst[Indices] = value.value[Indices]), ...);
    return dst;
}
template <size_t N>
INLINE char* write_string (char* dst, const StringLiteral<N>& value) {
    _write_sl(dst, value, std::make_index_sequence<N>{});
    return dst + N - 1;
}
template <size_t N>
INLINE char* _write_string (char* dst, const StringLiteral<N>& value, const Buffer&) {
    return write_string(dst, value);
}

template <size_t N>
INLINE size_t get_str_size (const StringLiteral<N>&) {
    return N - 1;
}


INLINE char* write_string (char* dst, const std::string_view& value) {
    std::memcpy(dst, value.data(), value.size());
    return dst + value.size();
}
INLINE char* _write_string (char* dst, const std::string_view& value, const Buffer&) {
    return write_string(dst, value);
}

INLINE constexpr size_t get_str_size (const std::string_view& str) {
    return str.size();
}


INLINE char* write_string (char* dst, uint64_t value) {
    if (value == 0) {
        *dst = '0';
        return dst + 1;
    } else {
        const uint8_t i = fast_math::log10_unsafe(value);
        char* const end = dst + i + 1;
        dst += i;
        *(dst--) = '0' + (value % 10);
        value /= 10;
        while (value > 0) {
            *(dst--) = '0' + (value % 10);
            value /= 10;
        }
        return end;
    }
}
INLINE char* _write_string (char* dst, uint64_t value, const Buffer&) {
    return write_string(dst, value);
}

INLINE size_t get_str_size (uint64_t value) {
    if (value == 0) {
        return 1;
    } else {
        return fast_math::log10_unsafe(value) + 1;
    }
}


template <StringLiteral seperator, typename ...T, size_t... Indices>
INLINE char* _write_code_parts_with_seperator (char* dst, _CodeParts<seperator, T...> value, std::index_sequence<Indices...>) {
    ((dst = write_string(write_string(dst, std::forward<std::tuple_element_t<Indices, decltype(value.values)>>(std::get<Indices>(value.values))), seperator)), ...);
    return dst;
};
template <StringLiteral seperator, typename ...T>
INLINE char* write_string(char* dst, _CodeParts<seperator, T...> value) {
    if constexpr (sizeof...(T) == 0) {
        return dst;
    } else {
        dst = _write_code_parts_with_seperator(dst, value, std::make_index_sequence<sizeof...(T) - 1>{});
        return write_string(dst, std::get<sizeof...(T) - 1>(value.values));
    }
}
template <StringLiteral seperator, typename ...T>
INLINE char* _write_string(char* dst, _CodeParts<seperator, T...> value, const Buffer&) {
    return write_string(dst, value);
}

template <StringLiteral seperator, typename ...T, size_t... Indices>
INLINE size_t _get_code_parts_size (_CodeParts<seperator, T...> value, std::index_sequence<Indices...>) {
    if constexpr (sizeof...(T) == 0) {
        return 0;
    } else {
        return (... + get_str_size(std::get<Indices>(value.values)));
    }
};

template <StringLiteral seperator, typename ...T>
INLINE size_t get_str_size (_CodeParts<seperator, T...> value) {
    return _get_code_parts_size(value, std::make_index_sequence<sizeof...(T)>{}) + (sizeof...(T) - 1) * seperator.size();
}


struct CodeData {
    CodeData (Buffer&& buffer, uint8_t indent) : buffer(std::move(buffer)), indent(indent) {}
    CodeData (CodeData&& other) noexcept = default;

    INLINE constexpr CodeData& operator = (CodeData&& other) = default;

    INLINE ~CodeData () = default;

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
            dst = _write_string(dst, std::forward<T>(strs), buffer)
        ), ...);
    }
    template <typename ...T>
    INLINE void _line (T&&... strs) {
        write_strs(strs..., "\n");
    }
};

struct ClosedCodeBlock {
    private:
    Buffer _buffer;
    public:
    ClosedCodeBlock (Buffer&& buffer) : _buffer(std::move(buffer)) {}
    INLINE const char* c_str () {
        *_buffer.get_next<char>() = 0;
        return reinterpret_cast<const char*>(_buffer.c_memory());
    }
    INLINE const Buffer::index_t size () { return _buffer.current_position(); }
    INLINE Buffer& buffer () { return _buffer; }
    INLINE void dispose () {
        _buffer.dispose();
    }
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
        return c_cast<Derived>(__CodeBlock<Last, Derived>({std::move(buffer), indent}));
    }

    INLINE Last end () {
        if constexpr (std::is_same_v<Last, ClosedCodeBlock>) {
            return ClosedCodeBlock(std::move(buffer));
        } else {
            _end();
            return c_cast<Last>(__CodeBlock<typename Last::__Last, typename Last::__Derived>({std::move(buffer), indent}));
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
    return If<Derived>({{std::move(buffer), indent}});
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
        return Case<Last>({{std::move(buffer), indent}});
    };

    INLINE auto _default () {
        _line("default: {");
        indent++;
        return Case<Last>({{std::move(buffer), indent}});
    }

    INLINE auto end () {
        indent--;
        _line("}");
        return c_cast<Last>(__CodeBlock<typename Last::__Last, typename Last::__Derived>({std::move(buffer), indent}));
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
    return Switch<Derived>({std::move(buffer), indent});
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
        return c_cast<Derived>(__Struct< Last, Derived>({std::move(buffer), indent}));
    }
    INLINE auto _public () {
        _line("public:");
        return c_cast<Derived>(__Struct<Last, Derived>({std::move(buffer), indent}));
    }
    INLINE auto _protected () {
        _line("protected:");
        return c_cast<Derived>(__Struct<Last, Derived>({std::move(buffer), indent}));
    }


    template <typename T, typename U>
    INLINE auto ctor (T&& args, U&& initializers) {
        _line(std::string_view{buffer.get(name.start), buffer.get(name.end)}, " (", args, ") : ", initializers, " {}");
        return EmptyCtor<Derived>({{std::move(buffer), indent}});
    }

    template <typename T, typename U>
    INLINE auto method (T&& type, U&& name) {
        _line(type, " ", name, " () {");
        indent++;
        return Method<Derived>({{std::move(buffer), indent}});
    }

    template <typename T, typename U, typename ...V>
    requires (sizeof...(V) > 0)
    INLINE auto method (T&& type, U&& name, Args<V...> args) {
        uint16_t indent_size = indent * 4;
        _line(type, " ", name, " (", args, ") {");
        indent++;
        return Method<Derived>({{std::move(buffer), indent}});
    }

    template <typename ...T, typename U, typename V>
    requires (sizeof...(T) > 0)
    INLINE auto method (Attributes<T...> attributes, U&& type, V&& name) {
        _line(attributes, " ", type, " ", name, " () {");
        indent++;
        return Method<Derived>({{std::move(buffer), indent}});
    }

    template <typename ...T, typename U, typename V, typename ...W>
    requires (sizeof...(T) > 0 && sizeof...(W) > 0)
    INLINE auto method (Attributes<T...> attributes, U&& type, V&& name, Args<W...> args) {
        _line(attributes, " ", type, " ", name, " (", args, ") {");
        indent++;
        return Method<Derived>({{std::move(buffer), indent}});
    }

    template <typename T, typename U>
    INLINE Derived field (T&& type, U&& name) {
        _line(type, " ", name, ";");
        return c_cast<Derived>(__Struct<Last, Derived>({std::move(buffer), indent}));
    } 

    template <typename ...T>
    INLINE auto _struct (T&&... strs);
};
template <typename Last>
INLINE auto Method<Last>::end ()  {
    this->_end();
    return c_cast<Last>(__Struct<typename Last::__Last, typename Last::__Derived>({std::move(this->buffer), this->indent}));
}

template <typename Last>
INLINE auto EmptyCtor<Last>::end ()  {
    return c_cast<Last>(__Struct<typename Last::__Last, typename Last::__Derived>({std::move(this->buffer), this->indent}));
}

template <typename Last>
struct Struct : __Struct<Last, Struct<Last>> {
    INLINE auto end () {
        this->indent--;
        this->_line("};");
        return c_cast<Last>(__CodeBlock<typename Last::__Last, typename Last::__Derived>({std::move(this->buffer), this->indent}));
    }
};
template <typename Last, typename Derived>
template <typename ...T>
INLINE auto __CodeBlock<Last, Derived>::_struct (T&&... strs) {
    Buffer::Index<char> name_start_idx = buffer.position_idx<char>().add(7 + indent * 4);
    _line("struct ", strs..., " {");
    Buffer::Index<char> name_end_idx = buffer.position_idx<char>().sub(3);
    indent++;
    return Struct<Derived>({{std::move(buffer), indent}, {name_start_idx, name_end_idx}});
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
        return c_cast<Last>(__Struct<typename Last::__Last, typename Last::__Derived>({std::move(this->buffer), this->indent}));
    }
};
template <typename Last, typename Derived>
template <typename ...T>
INLINE auto __Struct<Last, Derived>::_struct (T&&... strs) {
    Buffer::Index<char> name_start_idx = buffer.position_idx<char>().add(7 + indent * 4);
    _line("struct ", strs..., " {");
    Buffer::Index<char> name_end_idx = buffer.position_idx<char>().sub(3);
    indent++;
    return NestedStruct<Derived>({{std::move(buffer), indent}, {name_start_idx, name_end_idx}});
}

struct Empty {};
struct UnknownCodeBlock : CodeBlock<Empty> {};
struct __UnknownStruct : __Struct<Empty, __UnknownStruct> {};
typedef Struct<Empty> UnknownStruct;
typedef NestedStruct<Empty> UnknownNestedStruct;

INLINE CodeBlock<ClosedCodeBlock> create_code (Buffer&& buffer) {
    return {CodeData{std::move(buffer), 0}};
}

}