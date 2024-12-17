#pragma once
#include "base.cpp"
#include <cstdint>
#include <string>
#include <cstring>
#include "memory.cpp"

namespace codegen {

constexpr uint8_t CODE_INDENT_SPACES = 4;

template <uint8_t indent, size_t N>
INLINE void _add (const char (&line)[N], Buffer &buffer) {
    constexpr uint16_t indent_size = indent * 4;
    constexpr size_t line_size = N - 1;
    constexpr size_t str_size = indent_size + line_size + 1;
    auto str = buffer.get(buffer.get_next_multi_byte<char>(str_size));
    for (uint16_t i = 0; i < indent_size; i++) {
        str[i] = ' ';
    }
    std::memcpy(str + indent_size, line, line_size);
    str[line_size + indent_size] = '\n';
}

template <uint8_t indent>
INLINE void _add (std::string line, Buffer &buffer) {
    constexpr uint16_t indent_size = indent * 4;
    size_t line_size = line.size();
    size_t str_size = indent_size + line_size + 1;
    auto str = buffer.get(buffer.get_next_multi_byte<char>(str_size));
    for (uint16_t i = 0; i < indent_size; i++) {
        str[i] = ' ';
    }
    std::memcpy(str + indent_size, line.c_str(), line_size);
    str[line_size + indent_size] = '\n';
}



#define ADD(INDENT, LINE) _add<INDENT>(LINE, this->buffer)



struct CodeData {
    CodeData (Buffer buffer) : buffer(buffer) {}
    protected:
    Buffer buffer;
};

struct ClosedCodeBlock {
    private:
    Buffer buffer;
    public:
    ClosedCodeBlock (Buffer buffer) : buffer(buffer) {}
    INLINE const char* c_str () {
        *buffer.get_next<char>() = 0;
        return reinterpret_cast<const char*>(buffer.c_memory());
    }
    INLINE const size_t size () { return buffer.current_position(); }
    
};

template <size_t indent, typename Last, typename Derived>
struct __CodeBlock : CodeData {
    using __Last = Last;
    using __Derived = Derived;

    INLINE auto _if (std::string condition);
    INLINE auto _switch (std::string key);
    INLINE auto _struct (std::string name);

    INLINE Derived add (std::string line) {
        ADD(indent, line);
        return c_cast<Derived>(__CodeBlock<indent, Last, Derived>({this->buffer}));
    }

    INLINE Last end () {
        if constexpr (std::is_same_v<Last, ClosedCodeBlock>) {
            return ClosedCodeBlock(this->buffer);
        } else {
            ADD(indent - 1, "}");
            return c_cast<Last>(__CodeBlock<indent - 1, typename Last::__Last, typename Last::__Derived>({this->buffer}));
        }
    }
};


template <size_t indent, typename Last>
struct CodeBlock : __CodeBlock<indent, Last, CodeBlock<indent, Last>> {};


template <size_t indent, typename Last>
struct If : __CodeBlock<indent, Last, If<indent, Last>> {
    INLINE auto _else () {
        ADD(indent - 1, "} else {");
        return CodeBlock<indent, Last>({{this->buffer}});
    }
};
template <size_t indent, typename Last, typename Derived>
INLINE auto __CodeBlock<indent, Last, Derived>::_if (std::string condition) {
    ADD(indent, "if (" + condition + ") {");
    return If<indent + 1, Derived>({{this->buffer}});
};



template <size_t indent, typename Last>
struct Case : __CodeBlock<indent, Last, Case<indent, Last>> {
    INLINE auto end ();
};

template <size_t indent, typename Last>
struct Switch : CodeData {
    INLINE auto _case(std::string value) {
        ADD(indent, "case " + value + ": {");
        return Case<indent + 1, Last>({{buffer}});
    };

    INLINE auto _default () {
        ADD(indent, "default: {");
        return Case<indent + 1, Last>({{buffer}});
    }

    INLINE auto end () {
        ADD(indent - 1, "}");
        return c_cast<Last>(__CodeBlock<indent - 1, typename Last::__Last, typename Last::__Derived>({this->buffer}));
    }
};
template <size_t indent, typename Last>
INLINE auto Case<indent, Last>::end ()  {
    ADD(indent - 1, "}");
    return Switch<indent - 1, Last>({this->buffer});
}
template <size_t indent, typename Last, typename Derived>
INLINE auto __CodeBlock<indent, Last, Derived>::_switch (std::string key) {
    ADD(indent, "switch (" + key + ") {");
    return Switch<indent + 1, Derived>({this->buffer});
};

template <size_t indent, typename Last>
struct Method : __CodeBlock<indent, Last, Method<indent, Last>> {
    INLINE auto end ();
};

template <size_t indent, typename Last, typename Derived>
struct __Struct : CodeData {
    using __Last = Last;
    using __Derived = Derived;

    INLINE auto method (std::string attributes, std::string return_type, std::string name) {
        return __method(attributes + " " + return_type + " " + name + " () {");
    }
    INLINE auto method (std::string return_type, std::string name) {
        return __method(return_type + " " + name + " () {");
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
    INLINE auto __method (std::string method_qualifier) {
        ADD(indent, method_qualifier);
        return Method<indent + 1, Derived>({{buffer}});
    }
    INLINE Derived __field (std::string field_qualifier) {
        ADD(indent, field_qualifier);
        return c_cast<Derived>(__Struct<indent, Last, Derived>({this->buffer}));
    }
};
template <size_t indent, typename Last>
INLINE auto Method<indent, Last>::end ()  {
    ADD(indent - 1, "}");
    return c_cast<Last>(__Struct<indent - 1, typename Last::__Last, typename Last::__Derived>({this->buffer}));
}

template <size_t indent, typename Last>
struct Struct : __Struct<indent, Last, Struct<indent, Last>> {
    INLINE auto end () {
        ADD(indent - 1, "};");
        return c_cast<Last>(__CodeBlock<indent - 1, typename Last::__Last, typename Last::__Derived>({this->buffer}));
    }
};
template <size_t indent, typename Last, typename Derived>
INLINE auto __CodeBlock<indent, Last, Derived>::_struct (std::string name) {
    ADD(indent, "struct " + name + " {");
    return Struct<indent + 1, Derived>({this->buffer});
};
template <size_t indent, typename Last>
struct NestedStruct : __Struct<indent, Last, NestedStruct<indent, Last>> {
    INLINE auto end () {
        ADD(indent - 1, "};");
        return __end();
    }

    INLINE auto end (std::string name) {
        ADD(indent - 1, "} " + name + ";");
        return __end();
    }

    private:
    INLINE auto __end () {
        return c_cast<Last>(__Struct<indent - 1, typename Last::__Last, typename Last::__Derived>({this->buffer}));
    }
};
template <size_t indent, typename Last, typename Derived>
INLINE auto __Struct<indent, Last, Derived>::_struct (std::string name) {
    ADD(indent, "struct " + name + " {");
    return NestedStruct<indent + 1, Derived>({this->buffer});
}
template <size_t indent, typename Last, typename Derived>
INLINE auto __Struct<indent, Last, Derived>::_struct (std::string attributes, std::string name) {
    ADD(indent, attributes + " struct " + name + " {");
    return NestedStruct<indent + 1, Derived>({this->buffer});
}

template <size_t N>
INLINE auto create_code (uint8_t (&memory)[N]) {
    return CodeBlock<0, ClosedCodeBlock>({{Buffer(memory)}});
}

INLINE void test () {
    for (size_t i = 0; i < 5000000; i++) {
        uint8_t mem[5000];
        auto res = create_code(mem)
            .add("aawdw")
            ._struct("naur")
                ._struct("const", "a")
                    .field("int", "a")
                .end()
                
                .method("void", "a")
                    .add("aawdw")
                    ._if("true")
                    .end()
                .end()
            .end()
            ._if("true")
                .add("aawdw")
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
        printf("%s\n", res);
    }
}

}