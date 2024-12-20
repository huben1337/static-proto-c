#pragma once
#include <cstdint>
#include "base.cpp"

template <typename T>
INLINE size_t get_padding(size_t position) {
    size_t mod = position % alignof(T);
    size_t padding = (alignof(T) - mod) & (alignof(T) - 1);
    return padding;
}

template <typename T>
INLINE T* get_padded (auto* that) {
    size_t address = reinterpret_cast<size_t>(that);
    size_t padding = get_padding<T>(address);
    return std::assume_aligned<alignof(T)>(reinterpret_cast<T*>(address + padding));
}

template <typename T>
INLINE T* get_padded (size_t address) {
    size_t padding = get_padding<T>(address);
    return std::assume_aligned<alignof(T)>(reinterpret_cast<T*>(address + padding));
}

template <typename T, typename U>
INLINE T* get_extended(auto* that) {
    return get_padded<T>(reinterpret_cast<size_t>(that) + sizeof_v<U>);
}

template <typename T, typename Base>
struct __CreateExtendedResult {
    Buffer::Index<T> extended;
    Buffer::Index<Base> base;
};
template <typename T, typename Base>
INLINE __CreateExtendedResult<T, Base> __create_extended (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position() + sizeof_v<Base>);
    Buffer::Index<Base> base_idx = buffer.get_next_multi_byte<Base>(sizeof_v<Base> + padding + sizeof_v<T>);
    auto extended_idx = Buffer::Index<T>{static_cast<uint32_t>(base_idx.value + padding + sizeof_v<Base>)};
    return {extended_idx, base_idx};
}

template <typename T, typename Base>
struct CreateExtendedResult {
    T* extended;
    Base* base;
};
template <typename T, typename Base>
INLINE CreateExtendedResult<T, Base> create_extended (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position() + sizeof_v<Base>);
    Buffer::Index<Base> base_idx = buffer.get_next_multi_byte<Base>(sizeof_v<Base> + padding + sizeof_v<T>);
    auto extended_idx = Buffer::Index<T>{static_cast<uint32_t>(base_idx.value + padding + sizeof_v<Base>)};
    return {buffer.get(extended_idx), buffer.get(base_idx)};
}