#pragma once

#include "base.cpp"
#include <cstdint>
#include <string>
#include <cstring>
#include <cmath>
#include "memory.cpp"

namespace codegen {

constexpr uint8_t CODE_INDENT_SPACES = 4;

template <size_t N>
INLINE void _line (const char (&line)[N], uint8_t indent, Buffer &buffer) {
    uint16_t indent_size = indent * 4;
    size_t line_size = N - 1;
    size_t str_size = indent_size + line_size + 1;
    auto str = buffer.get(buffer.get_next_multi_byte<char>(str_size));
    for (uint16_t i = 0; i < indent_size; i++) {
        str[i] = ' ';
    }
    std::memcpy(str + indent_size, line, line_size);
    str[line_size + indent_size] = '\n';
}

INLINE void _line (std::string line, uint8_t indent, Buffer &buffer) {
    uint16_t indent_size = indent * 4;
    size_t line_size = line.size();
    size_t str_size = indent_size + line_size + 1;
    auto str = buffer.get(buffer.get_next_multi_byte<char>(str_size));
    for (uint16_t i = 0; i < indent_size; i++) {
        str[i] = ' ';
    }
    std::memcpy(str + indent_size, line.c_str(), line_size);
    str[line_size + indent_size] = '\n';
}



#define LINE(LINE) _line(LINE, this->indent, this->buffer)

struct CodeData {
    CodeData (Buffer buffer, uint8_t indent) : buffer(buffer), indent(indent) {}
    protected:
    Buffer buffer;
    uint8_t indent;
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
    INLINE const size_t size () { return _buffer.current_position(); }
    INLINE Buffer buffer () { return _buffer; }
    INLINE void dispose () { _buffer.free(); }
    
};

template <typename Last, typename Derived>
struct __CodeBlock : CodeData {
    using __Last = Last;
    using __Derived = Derived;

    INLINE auto _if (std::string condition);
    INLINE auto _switch (std::string key);
    INLINE auto _struct (std::string name);

    INLINE auto operator() (std::string value);

    INLINE Derived line (std::string line) {
        LINE(line);
        return c_cast<Derived>(__CodeBlock<Last, Derived>({this->buffer, this->indent}));
    }

    INLINE Last end () {
        if constexpr (std::is_same_v<Last, ClosedCodeBlock>) {
            return ClosedCodeBlock(this->buffer);
        } else {
            this->indent--;
            LINE("}");
            return c_cast<Last>(__CodeBlock<typename Last::__Last, typename Last::__Derived>({this->buffer, this->indent}));
        }
    }
};

template <typename Last>
struct Line : CodeData {
    INLINE Line (Buffer buffer, uint8_t indent) : CodeData(buffer, indent) {}
    /**
     * Concatenate the given string to the line of code stored in this object
     * and return a new Line object with the concatenated line.
     *
     * @param value the string to concatenate
     * @return a new Line object
     */
    INLINE auto operator() (std::string value) {
        size_t size = value.size();
        auto str = buffer.get(buffer.get_next_multi_byte<char>(size));
        std::memcpy(str, value.c_str(), size);
        return Line<Last>(this->buffer, this->indent);
    }
    INLINE auto operator() (std::string value, Buffer::Index<char>* idx) {
        size_t size = value.size();
        auto str_idx = buffer.get_next_multi_byte<char>(size);
        *idx = str_idx;
        auto str = buffer.get(str_idx);
        std::memcpy(str, value.c_str(), size);
        return Line<Last>(this->buffer, this->indent);
    }
    template <UnsignedIntegral T>
    INLINE auto operator() (T value) {
        uint8_t length = std::log10(value) + 1;
        auto str = buffer.get(buffer.get_next_multi_byte<char>(length));
        uint8_t i = length - 1;
        while (value > 0) {
            str[i--] = '0' + (value % 10);
            value /= 10;
        }
        return Line<Last>(this->buffer, this->indent);
    }
    /**
     * Write the line of code stored in this object to the code buffer, and return
     * a CodeBlock that can be used to generate more code.
     *
     * @return a CodeBlock
     */
    INLINE auto nl() {
        *buffer.get_next<char>() = '\n';
        return c_cast<Last>(__CodeBlock<typename Last::__Last, typename Last::__Derived>({this->buffer, this->indent}));
    };
};
template <typename Last, typename Derived>
INLINE auto __CodeBlock<Last, Derived>::operator() (std::string value) {
    uint16_t indent_size = this->indent * 4;
    size_t size = value.size();
    size_t str_size = indent_size + size;
    auto str = buffer.get(buffer.get_next_multi_byte<char>(str_size));
    for (uint16_t i = 0; i < indent_size; i++) {
        str[i] = ' ';
    }
    std::memcpy(str + indent_size, value.c_str(), size);
    return Line<Derived>(this->buffer, this->indent);
}

template <typename Last>
struct CodeBlock : __CodeBlock<Last, CodeBlock<Last>> {};


template <typename Last>
struct If : __CodeBlock<Last, If<Last>> {
    INLINE auto _else () {
        this->indent--;
        LINE("} else {");
        return CodeBlock<Last>({{this->buffer, this->indent}});
    }
};
template <typename Last, typename Derived>
INLINE auto __CodeBlock<Last, Derived>::_if (std::string condition) {
    LINE("if (" + condition + ") {");
    this->indent++;
    return If<Derived>({{this->buffer, this->indent}});
};



template <typename Last>
struct Case : __CodeBlock<Last, Case<Last>> {
    INLINE auto end ();
};

template <typename Last>
struct Switch : CodeData {
    INLINE auto _case(std::string value) {
        LINE("case " + value + ": {");
        indent++;
        return Case<Last>({{buffer, indent}});
    };

    INLINE auto _default () {
        LINE("default: {");
        indent++;
        return Case<Last>({{buffer, indent}});
    }

    INLINE auto end () {
        indent--;
        LINE("}");
        return c_cast<Last>(__CodeBlock<typename Last::__Last, typename Last::__Derived>({this->buffer, this->indent}));
    }
};
template <typename Last>
INLINE auto Case<Last>::end ()  {
    this->indent--;
    LINE("}");
    return Switch<Last>({this->buffer, this->indent});
}
template <typename Last, typename Derived>
INLINE auto __CodeBlock<Last, Derived>::_switch (std::string key) {
    LINE("switch (" + key + ") {");
    this->indent++;
    return Switch<Derived>({this->buffer, this->indent});
};

template <typename Last>
struct Method : __CodeBlock<Last, Method<Last>> {
    INLINE auto end ();
};

template <typename Last, typename Derived>
struct __Struct : CodeData {
    using __Last = Last;
    using __Derived = Derived;

    std::string name;

    INLINE auto _private () {
        LINE("private:");
        return c_cast<Derived>(__Struct< Last, Derived>({this->buffer, this->indent}));
    }
    INLINE auto _public () {
        LINE("public:");
        return c_cast<Derived>(__Struct<Last, Derived>({this->buffer, this->indent}));
    }
    INLINE auto _protected () {
        LINE("protected:");
        return c_cast<Derived>(__Struct<Last, Derived>({this->buffer, this->indent}));
    }

    INLINE auto ctor (std::string args, std::string initializers) {
        LINE(name + " (" + args + ") :" + initializers + " {}");
        return c_cast<Derived>(__Struct<Last, Derived>({this->buffer, this->indent}));
    }

    INLINE auto method (std::string method_qualifier, std::string args = "") {
        return __method(method_qualifier + " (" + args + ") {");
    }

    INLINE auto field (std::string attributes, std::string type, std::string name) {
        return __field(attributes + " " + type + " " + name + ";");
    }
    INLINE auto field (std::string type, std::string name) {
       return __field(type + " " + name + ";");
    }

    INLINE auto _struct (std::string name);
    INLINE auto _struct (std::string attributes, std::string name);

    private:
    INLINE auto __method (std::string str) {
        LINE(str);
        indent++;
        return Method<Derived>({{buffer, indent}});
    }
    INLINE Derived __field (std::string str) {
        LINE(str);
        return c_cast<Derived>(__Struct<Last, Derived>({this->buffer, this->indent}));
    }
};
template <typename Last>
INLINE auto Method<Last>::end ()  {
    this->indent--;
    LINE("}");
    return c_cast<Last>(__Struct<typename Last::__Last, typename Last::__Derived>({this->buffer, this->indent}));
}

template <typename Last>
struct Struct : __Struct<Last, Struct<Last>> {
    INLINE auto end () {
        this->indent--;
        LINE("};");
        return c_cast<Last>(__CodeBlock<typename Last::__Last, typename Last::__Derived>({this->buffer, this->indent}));
    }
};
template <typename Last, typename Derived>
INLINE auto __CodeBlock<Last, Derived>::_struct (std::string name) {
    LINE("struct " + name + " {");
    this->indent++;
    return Struct<Derived>({{this->buffer, this->indent}, name});
};
template <typename Last>
struct NestedStruct : __Struct<Last, NestedStruct<Last>> {
    INLINE auto end () {
        this->indent--;
        LINE("};");
        return __end();
    }

    INLINE auto end (std::string name) {
        this->indent--;
        LINE("} " + name + ";");
        return __end();
    }

    private:
    INLINE auto __end () {
        return c_cast<Last>(__Struct<typename Last::__Last, typename Last::__Derived>({this->buffer, this->indent}));
    }
};
template <typename Last, typename Derived>
INLINE auto __Struct<Last, Derived>::_struct (std::string name) {
    LINE("struct " + name + " {");
    this->indent++;
    return NestedStruct<Derived>({{this->buffer, this->indent}, name});
}
template <typename Last, typename Derived>
INLINE auto __Struct<Last, Derived>::_struct (std::string attributes, std::string name) {
    LINE(attributes + " struct " + name + " {");
    this->indent++;
    return NestedStruct<Derived>({{this->buffer, this->indent}, name});
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

INLINE void test () {
    for (size_t i = 0; i < 5000000; i++) {
        uint8_t mem[5000];
        auto res = create_code(mem)
            ("aawdw").nl()
            ("a + ")("b").nl()

            ._struct("naur")
                ._struct("const", "a")
                    .field("int", "a")
                .end()
                
                .method("void", "a")
                    ("aawdw").nl()
                    ._if("true")
                    .end()
                .end()
            .end()
            ._if("true")
                ("aawdw").nl()
                ._if("true")
                ._else()
                .end()
                ._switch("a")
                    ._case("1")
                    .end()
                    ._case("2")
                    .end()
                    ._default()
                    .end()
                .end()
            ._else()
                
            .end()
        .end().c_str();
        //printf("%s\n", res);
    }
}

}