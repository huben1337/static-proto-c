#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

#include "../container/memory.hpp"

namespace lexer {

template <typename T>
[[nodiscard]] constexpr size_t get_padding(uintptr_t position) {
    size_t mod = position % alignof(T);
    size_t padding = (alignof(T) - mod) & (alignof(T) - 1);
    return padding;
}

namespace _detail {
    template <typename T>
    [[nodiscard]] constexpr T& get_padded (uintptr_t address) {
        size_t padding = get_padding<T>(address);
        return *std::assume_aligned<alignof(T)>(reinterpret_cast<T*>(address + padding));
    }

}
template <typename T>
[[nodiscard]] constexpr T& get_padded (auto* that) {
    return _detail::get_padded<T>(reinterpret_cast<uintptr_t>(that));
}

template <typename T>
[[nodiscard]] inline Buffer::Index<T> create_padded (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position());
    Buffer::Index<T> idx = buffer.next_multi_byte<T>(sizeof(T) + padding);
    return idx.add(padding);
}

template <typename T>
inline void create_padded (Buffer &buffer, T&& t) {
    buffer.get(create_padded<std::remove_cvref_t<T>>(buffer)) = std::forward<T>(t);
}

template <typename T, typename Base>
requires (alignof(Base) == 1)
[[nodiscard]] constexpr T& get_extended (auto* that) {
    return _detail::get_padded<T>(reinterpret_cast<uintptr_t>(that) + sizeof(Base));
}

template <typename T, typename Base>
struct CreateExtendedResult {
    Buffer::Index<T> extended;
    Buffer::Index<Base> base;
};

template <typename T, typename Base, typename Next = void>
requires (alignof(Base) == 1 && std::is_void_v<Next>)
[[nodiscard]] inline CreateExtendedResult<T, Base> create_extended (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position() + sizeof(Base));
    Buffer::Index<Base> base_idx = buffer.next_multi_byte<Base>(sizeof(Base) + padding + sizeof(T));
    Buffer::Index<T> extended_idx {static_cast<Buffer::index_t>(base_idx.value + padding + sizeof(Base))};
    return {extended_idx, base_idx};
}

template <typename T, typename Base, typename Next>
requires (alignof(Base) == 1 && !std::is_void_v<Next>)
[[nodiscard]] inline CreateExtendedResult<T, Base> create_extended (Buffer &buffer) {
    Buffer::index_t pos = buffer.current_position();
    size_t padding_before = get_padding<T>(pos + sizeof(Base));
    size_t size = sizeof(Base) + padding_before + sizeof(T);
    size_t padding_after = get_padding<Next>(pos + size);
    Buffer::Index<Base> base_idx = buffer.next_multi_byte<Base>(size + padding_after);
    Buffer::Index<T> extended_idx {static_cast<Buffer::index_t>(base_idx.value + padding_before + sizeof(Base))};
    return {extended_idx, base_idx};
}

template <typename Base, typename T, typename Next = void>
requires (alignof(Base) == 1)
inline void create_extended (Buffer &buffer, Base&& base, T&& extended) {
    auto created = create_extended<std::remove_cvref_t<T>, std::remove_cvref_t<Base>, Next>(buffer);
    buffer.get(created.base) = std::forward<Base>(base);
    buffer.get(created.extended) = std::forward<T>(extended);
}


}