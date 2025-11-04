#pragma once

#include <cstddef>
#include <cstdlib>
#include <string_view>

#include "../container/memory.hpp"
#include "../util/logger.hpp"
#include "../util/stringify.hpp"


namespace estd {

    namespace {
        Buffer error_buffer = BUFFER_INIT_STACK(128);
    }

    template <bool use_exceptions, typename ...T>
    requires (use_exceptions)
    [[noreturn]] constexpr void throw_error (T&&... args);

    class error : public std::exception {
    private:
        Buffer::View<const char> msg_view;

        constexpr explicit error (Buffer::View<const char> msg_view) : msg_view(msg_view) {}

    public:
        error (const error&) = delete;
        error (error&&) = delete;

        error& operator = (const error&) = delete;
        error& operator = (error&&) = delete;

        constexpr ~error () override {
            error_buffer.go_back(msg_view.size());
        }

        [[nodiscard]] constexpr const char* what () const noexcept override { return msg_view.begin(error_buffer); }

        [[nodiscard]] constexpr std::basic_string_view<const char> msg () const noexcept { return {msg_view.begin(error_buffer), msg_view.size()}; }

        template <bool use_exceptions, typename ...T>
        requires (use_exceptions)
        friend constexpr void throw_error (T&&... args);
    };

    template <bool use_exceptions, typename ...T>
    requires(use_exceptions)
    [[noreturn]] constexpr void throw_error (T&&... args) {
        size_t msg_size = (... + stringify::get_str_size(args)) + 1;
        Buffer::Index<char> start_idx = error_buffer.next_multi_byte<char>(msg_size);
        char* dst = error_buffer.get(start_idx);
        ((
            dst = stringify::_write_string(dst, std::forward<T>(args), error_buffer)
        ), ...);
        *dst = '\0';
        throw error{{start_idx, static_cast<Buffer::index_t>(msg_size)}};
    }

    template <bool use_exceptions, typename ...T>
    requires(!use_exceptions)
    [[noreturn]] constexpr void throw_error (T&&... args) noexcept {
        logger::error(std::forward<T>(args)...);
        std::exit(1);
    }
}