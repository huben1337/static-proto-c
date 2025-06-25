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
#include "string_literal.cpp"
#include "memory.cpp"
#include "fast_math.cpp"

namespace codegen {

template <StringLiteral seperator, typename ...T>
struct _CodeParts : std::tuple<T...>  {
    INLINE constexpr _CodeParts (T&&... args) : std::tuple<T...>(std::forward<T>(args)...) {}
};

template <StringLiteral separator, typename... T>
auto make_code_parts(T&&... args) {
    return _CodeParts<separator, T...>(std::forward<T>(args)...);
}

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

template <typename... T>
struct is_code_parts_t : std::false_type {};
template <StringLiteral seperator, typename... T>
struct is_code_parts_t<_CodeParts<seperator, T...>> : std::true_type {};

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

// int x = 0;
// auto a = make_code_parts<"">("as_", 1, x);
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
INLINE char* _write_code_parts_with_seperator (char* dst, const _CodeParts<seperator, T...>& value, std::index_sequence<Indices...>) {
    ((dst = write_string(write_string(dst, std::get<Indices>(value)), seperator)), ...);
    return dst;
};
template <StringLiteral seperator, typename ...T>
INLINE char* write_string(char* dst, const _CodeParts<seperator, T...>& value) {
    if constexpr (sizeof...(T) == 0) {
        return dst;
    } else {
        dst = _write_code_parts_with_seperator(dst, value, std::make_index_sequence<sizeof...(T) - 1>{});
        return write_string(dst, std::get<sizeof...(T) - 1>(value));
    }
}
template <StringLiteral seperator, typename ...T>
INLINE char* _write_string(char* dst, const _CodeParts<seperator, T...>& value, const Buffer&) {
    return write_string(dst, value);
}

template <StringLiteral seperator, typename ...T, size_t... Indices>
INLINE size_t _get_code_parts_size (const _CodeParts<seperator, T...>& value, std::index_sequence<Indices...>) {
    if constexpr (sizeof...(T) == 0) {
        return 0;
    } else {
        return (... + get_str_size(std::get<Indices>(value)));
    }
};

template <StringLiteral seperator, typename ...T>
INLINE size_t get_str_size (const _CodeParts<seperator, T...>& value) {
    return _get_code_parts_size(value, std::make_index_sequence<sizeof...(T)>{}) + (sizeof...(T) - 1) * seperator.size();
}


struct CodeData {
    INLINE constexpr CodeData (Buffer&& buffer, uint8_t indent) : buffer(std::move(buffer)), indent(indent) {}

    CodeData (const CodeData& other) noexcept = delete;
    CodeData& operator = (const CodeData& other) = delete;

    INLINE constexpr CodeData (CodeData&& other) noexcept = default;    
    INLINE constexpr CodeData& operator = (CodeData&& other) = default;

    protected:
    Buffer buffer;
    uint8_t indent;

    template <typename ...T>
    INLINE void write_strs (T&&... strs) {
        uint16_t indent_size = indent * 4;
        size_t size = (... + get_str_size(strs)) + indent_size;
        Buffer::Index<char> dst_idx = buffer.next_multi_byte<char>(size);
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

#define CODE_DATA_CTOR_ARGS {std::move(this->buffer), this->indent}

struct ClosedCodeBlock {
    private:
    Buffer buffer;

    public:
    ClosedCodeBlock (Buffer&& buffer) : buffer(std::move(buffer)) {}

    INLINE const char* const& data () const { return reinterpret_cast<const char *const &>(buffer.data()); }
    
    INLINE const Buffer::index_t& size () const { return buffer.current_position(); }

    INLINE void dispose () {
        buffer.dispose();
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
        return {{CODE_DATA_CTOR_ARGS}};
    }

    INLINE auto end () {
        if constexpr (std::is_same_v<Last, ClosedCodeBlock>) {
            return ClosedCodeBlock(std::move(buffer));
        } else {
            _end();
            return Last{{CODE_DATA_CTOR_ARGS}};
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
    INLINE CodeBlock<Last> _else () {
        this->indent--;
        this->_line("} else {");
        return {{CODE_DATA_CTOR_ARGS}};
    }
};
template <typename Last, typename Derived>
INLINE auto __CodeBlock<Last, Derived>::_if (std::string_view condition) {
    _line("if (", condition, ") {");
    indent++;
    return If<Derived>{{CODE_DATA_CTOR_ARGS}};
};



template <typename Last>
struct Case : __CodeBlock<Last, Case<Last>> {
    INLINE auto end ();
};

template <typename Last>
struct Switch : CodeData {
    INLINE Case<Last> _case(std::string_view value) {
        _line("case ", value, ": {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    };

    INLINE Case<Last> _default () {
        _line("default: {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    }

    INLINE Last end () {
        indent--;
        _line("}");
        return {{CODE_DATA_CTOR_ARGS}};
    }
};
template <typename Last>
INLINE auto Case<Last>::end ()  {
    this->_end();
    return Switch<Last>{{CODE_DATA_CTOR_ARGS}};
}
template <typename Last, typename Derived>
INLINE auto __CodeBlock<Last, Derived>::_switch (std::string_view key) {
    _line("switch (", key, ") {");
    indent++;
    return Switch<Derived>{{CODE_DATA_CTOR_ARGS}};
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

    Buffer::View<char> name;

    INLINE Derived _private () {
        _line("private:");
        return {{CODE_DATA_CTOR_ARGS}};
    }
    INLINE Derived _public () {
        _line("public:");
        return {{CODE_DATA_CTOR_ARGS}};
    }
    INLINE Derived _protected () {
        _line("protected:");
        return {{CODE_DATA_CTOR_ARGS}};
    }


    template <typename T, typename U>
    INLINE EmptyCtor<Derived> ctor (T&& args, U&& initializers) {
        _line(std::string_view{name.begin(buffer), name.length}, " (", args, ") : ", initializers, " {}");
        return {{CODE_DATA_CTOR_ARGS}};
    }

    template <typename T, typename U>
    INLINE Method<Derived> method (T&& type, U&& name) {
        _line(type, " ", name, " () {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    }

    template <typename T, typename U, typename ...V>
    requires (sizeof...(V) > 0)
    INLINE Method<Derived> method (T&& type, U&& name, const Args<V...>& args) {
        _line(type, " ", name, " (", args, ") {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    }

    template <typename ...T, typename U, typename V>
    requires (sizeof...(T) > 0)
    INLINE Method<Derived> method (const Attributes<T...>& attributes, U&& type, V&& name) {
        _line(attributes, " ", type, " ", name, " () {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    }

    template <typename ...T, typename U, typename V, typename ...W>
    requires (sizeof...(T) > 0 && sizeof...(W) > 0)
    INLINE Method<Derived> method (const Attributes<T...>& attributes, U&& type, V&& name, const Args<W...>& args) {
        _line(attributes, " ", type, " ", name, " (", args, ") {");
        indent++;
        return {{CODE_DATA_CTOR_ARGS}};
    }

    template <typename T, typename U>
    INLINE Derived field (T&& type, U&& name) {
        _line(type, " ", name, ";");
        return {{CODE_DATA_CTOR_ARGS}};
    } 

    template <typename ...T>
    INLINE auto _struct (T&&... strs);
};
template <typename Last>
INLINE auto Method<Last>::end ()  {
    this->_end();
    return Last{{CODE_DATA_CTOR_ARGS}};
}

template <typename Last>
INLINE auto EmptyCtor<Last>::end ()  {
    return Last{{CODE_DATA_CTOR_ARGS}};
}

template <typename Last>
struct Struct : __Struct<Last, Struct<Last>> {
    INLINE Last end () {
        this->indent--;
        this->_line("};");
        return {{CODE_DATA_CTOR_ARGS}};
    }
};
template <typename Last, typename Derived>
template <typename ...T>
INLINE auto __CodeBlock<Last, Derived>::_struct (T&&... strs) {
    Buffer::Index<char> name_start_idx = buffer.position_idx<char>().add(indent * 4 + "struct "_sl.size());
    _line("struct ", strs..., " {");
    Buffer::Index<char> name_end_idx = buffer.position_idx<char>().sub(" {"_sl.size());
    indent++;
    return Struct<Derived>{{CODE_DATA_CTOR_ARGS, {name_start_idx, name_end_idx}}};
};
template <typename Last>
struct NestedStruct : __Struct<Last, NestedStruct<Last>> {
    INLINE Last end () {
        this->indent--;
        this->_line("};");
        return {{CODE_DATA_CTOR_ARGS}};
    }

    INLINE Last end (std::string_view name) {
        this->indent--;
        this->_line("}; ", name, ";");
        return {{CODE_DATA_CTOR_ARGS}};
    }
};
template <typename Last, typename Derived>
template <typename ...T>
INLINE auto __Struct<Last, Derived>::_struct (T&&... strs) {
    Buffer::Index<char> name_start_idx = buffer.position_idx<char>().add(7 + indent * 4);
    _line("struct ", strs..., " {");
    Buffer::Index<char> name_end_idx = buffer.position_idx<char>().sub(3);
    indent++;
    return NestedStruct<Derived>{{CODE_DATA_CTOR_ARGS, {name_start_idx, name_end_idx}}};
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