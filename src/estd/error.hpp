#pragma once

#include <cstddef>
#include <cstdlib>
#include <span>
#include <string_view>

#include "../container/memory.hpp"
#include "../util/logger.hpp"
#include "../util/stringify.hpp"


namespace estd {

    using MsgBuf = Memory<size_t, char>;
    namespace _detail {
        static MsgBuf error_buffer = MEMORY_INIT_STACK(size_t, char, 128);
    }

    template <bool use_exceptions, typename ...T>
    requires (use_exceptions)
    [[noreturn]] constexpr void throw_error (T&&... args);

    class error : public std::exception {
    private:
        MsgBuf::View<const char> msg_view;

        constexpr explicit error (MsgBuf::View<const char> msg_view) : msg_view(msg_view) {}

    public:
        error (const error&) = delete;
        error (error&&) = delete;

        error& operator = (const error&) = delete;
        error& operator = (error&&) = delete;

        constexpr ~error () override {
            _detail::error_buffer.go_back(msg_view.size());
        }

        [[nodiscard]] constexpr const char* what () const noexcept override { return &_detail::error_buffer.get(msg_view.start_idx); }

        [[nodiscard]] constexpr std::basic_string_view<const char> msg () const noexcept {
            const std::span<const char> msg = _detail::error_buffer.get(msg_view);
            return {msg.data(), msg.size()};
        }

        template <bool use_exceptions, typename ...T>
        requires (use_exceptions)
        friend constexpr void throw_error (T&&... args);
    };

    template <bool use_exceptions, typename ...T>
    requires(use_exceptions)
    [[noreturn]] constexpr void throw_error (T&&... args) {
        size_t msg_size = (... + stringify::get_str_size(args)) + 1;
        MsgBuf::Index<char> start_idx = _detail::error_buffer.next_multi_byte<char>(msg_size);
        char* dst = &_detail::error_buffer.get(start_idx);
        ((
            dst = stringify::_write_string(dst, std::forward<T>(args), _detail::error_buffer)
        ), ...);
        *dst = '\0';
        throw error{{start_idx, msg_size}};
    }

    template <bool use_exceptions, typename ...T>
    requires(!use_exceptions)
    [[noreturn]] constexpr void throw_error (T&&... args) noexcept {
        console.error(std::forward<T>(args)...);
        std::exit(1);
    }
}