#pragma once

#include <cstddef>
#include <stdexcept>
#include <concepts>
#include <string_view>

#include "memory.cpp"
#include "logger.cpp"
#include "codegen.cpp"

namespace _error_internal_namespace {
    alignas(4096) static char _mem[4096];
    static Buffer buffer = Buffer(_mem);
}

template <bool use_exceptions, typename ...T>
[[noreturn]] inline void throw_error (T&&... args) requires(use_exceptions);

class error : public std::exception {
private:

    class _msg {
    private:
        const Buffer::View<char> msg_span;
    public:
        _msg (const Buffer::View<char>& msg_span) : msg_span(msg_span) {}
        _msg (Buffer::View<char>&& msg_span) : msg_span(std::move(msg_span)) {}

        const char* begin () const { return msg_span.begin(_error_internal_namespace::buffer); }
        const char* end () const { return msg_span.end(_error_internal_namespace::buffer); }
        const char* data () const { return begin(); }
        const char* c_str () const { return data(); }
        size_t size () const { return msg_span.size(); }

        std::string_view to_sv () const { return {data(), size()}; }
    };
    
public:
    error (_msg&& msg) : msg(std::move(msg)) {}
    error (const error&) = delete;
    error (error&&) = delete;

    const _msg msg;

    error& operator = (const error&) = delete;
    error& operator = (error&&) = delete;

    ~error () noexcept override {
        _error_internal_namespace::buffer.go_back(msg.size());
    }

    const char* what () const noexcept override { return msg.begin(); }

    template <bool use_exceptions, typename ...T>
    friend void throw_error (T&&... args) requires(use_exceptions);
};

template <typename T>
concept CharDataHolder = requires (T holder) {
    { holder.data() } -> std::convertible_to<const char*>;
};

template <bool use_exceptions, CharDataHolder T>
[[noreturn]] inline void throw_runtime_error (const std::string& msg){
    throw std::runtime_error{msg};
}

template <bool use_exceptions, typename ...T>
[[noreturn]] inline void throw_error (T&&... args) requires(use_exceptions) {
    size_t msg_size = (... + codegen::get_str_size(args)) + 1;
    Buffer::Index<char> start_idx = _error_internal_namespace::buffer.next_multi_byte<char>(msg_size);
    char* dst = _error_internal_namespace::buffer.get(start_idx);
    ((
        dst = codegen::_write_string(dst, std::forward<T>(args), _error_internal_namespace::buffer)
    ), ...);
    *dst = '\0';
    throw error{Buffer::View<char>{start_idx, static_cast<Buffer::index_t>(msg_size)}};
}

template <bool use_exceptions, typename ...T>
[[noreturn]] inline void throw_error (T&&... args) noexcept requires(!use_exceptions) {
    logger::error(std::forward<T>(args)...);
    exit(1);
}