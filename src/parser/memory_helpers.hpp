#pragma once

#include <cstdint>

#include "../container/memory.hpp"

namespace lexer {

template <typename T>
[[nodiscard]] constexpr size_t get_padding(uintptr_t position) {
    size_t mod = position % alignof(T);
    size_t padding = (alignof(T) - mod) & (alignof(T) - 1);
    return padding;
}

template <typename T>
[[nodiscard]] constexpr T* get_padded (uintptr_t address) {
    size_t padding = get_padding<T>(address);
    return std::assume_aligned<alignof(T)>(reinterpret_cast<T*>(address + padding));
}

template <typename T>
[[nodiscard]] constexpr T* get_padded (auto* that) {
    return get_padded<T>(reinterpret_cast<uintptr_t>(that));
}

template <typename T>
[[nodiscard]] inline Buffer::Index<T> __create_padded (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position());
    Buffer::Index<T> idx = buffer.next_multi_byte<T>(sizeof(T) + padding);
    return idx.add(padding);
}

template <typename T>
[[nodiscard]] inline T* create_padded (Buffer &buffer) {
    return buffer.get_aligned(__create_padded<T>(buffer));
}

template <typename T, typename U>
[[nodiscard]] constexpr T* get_extended (auto* that) {
    return get_padded<T>(reinterpret_cast<uintptr_t>(that) + sizeof(U));
}

template <typename T, typename Base>
struct __CreateExtendedResult {
    Buffer::Index<T> extended;
    Buffer::Index<Base> base;
};

template <typename T, typename Base>
requires (alignof(Base) == 1)
[[nodiscard]] inline __CreateExtendedResult<T, Base> __create_extended (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position() + sizeof(Base));
    Buffer::Index<Base> base_idx = buffer.next_multi_byte<Base>(sizeof(Base) + padding + sizeof(T));
    Buffer::Index<T> extended_idx {static_cast<Buffer::index_t>(base_idx.value + padding + sizeof(Base))};
    return {extended_idx, base_idx};
}

template <typename T, typename Base, typename Next_T>
requires (alignof(Base) == 1)
[[nodiscard]] inline __CreateExtendedResult<T, Base> __create_extended (Buffer &buffer) {
    Buffer::index_t pos = buffer.current_position();
    size_t padding_before = get_padding<T>(pos + sizeof(Base));
    size_t size = sizeof(Base) + padding_before + sizeof(T);
    size_t padding_after = get_padding<Next_T>(pos + size);
    Buffer::Index<Base> base_idx = buffer.next_multi_byte<Base>(size + padding_after);
    Buffer::Index<T> extended_idx {static_cast<Buffer::index_t>(base_idx.value + padding_before + sizeof(Base))};
    return {extended_idx, base_idx};
}

template <typename T, typename Base>
struct CreateExtendedResult {
    T* extended;
    Base* base;
};

template <typename T, typename Base>
requires (alignof(Base) == 1)
[[nodiscard]] inline CreateExtendedResult<T, Base> create_extended (Buffer &buffer) {
    __CreateExtendedResult<T, Base> created = __create_extended<T, Base>(buffer);
    return {buffer.get_aligned(created.extended), buffer.get_aligned(created.base)};
}

template <typename T, typename Base, typename Next_T>
requires (alignof(Base) == 1)
[[nodiscard]] inline CreateExtendedResult<T, Base> create_extended (Buffer &buffer) {
    __CreateExtendedResult<T, Base> created = __create_extended<T, Base, Next_T>(buffer);
    return {buffer.get_aligned(created.extended), buffer.get_aligned(created.base)};
}

}