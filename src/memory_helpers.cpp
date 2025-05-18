#pragma once
#include "base.cpp"
#include "memory.cpp"

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

template <typename T>
INLINE Buffer::Index<T> __create_padded (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position());
    Buffer::Index<T> idx = buffer.next_multi_byte<T>(sizeof_v<T> + padding);
    return idx.add(padding);
}

template <typename T>
INLINE T* create_padded (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position());
    Buffer::Index<T> idx = buffer.next_multi_byte<T>(sizeof_v<T> + padding);
    return buffer.get_aligned(idx.add(padding));
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
requires (alignof(Base) == 1)
INLINE __CreateExtendedResult<T, Base> __create_extended (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position() + sizeof_v<Base>);
    Buffer::Index<Base> base_idx = buffer.next_multi_byte<Base>(sizeof_v<Base> + padding + sizeof_v<T>);
    Buffer::Index<T> extended_idx = {static_cast<Buffer::index_t>(base_idx.value + padding + sizeof_v<Base>)};
    return {extended_idx, base_idx};
}

template <typename T, typename Base, typename Next_T>
requires (alignof(Base) == 1)
INLINE __CreateExtendedResult<T, Base> __create_extended (Buffer &buffer) {
    Buffer::index_t pos = buffer.current_position();
    size_t padding_before = get_padding<T>(pos + sizeof_v<Base>);
    size_t size = sizeof_v<Base> + padding_before + sizeof_v<T>;
    size_t padding_after = get_padding<Next_T>(pos + size);
    Buffer::Index<Base> base_idx = buffer.next_multi_byte<Base>(size + padding_after);
    Buffer::Index<T> extended_idx = {static_cast<Buffer::index_t>(base_idx.value + padding_before + sizeof_v<Base>)};
    return {extended_idx, base_idx};
}

template <typename T, typename Base>
struct CreateExtendedResult {
    T* extended;
    Base* base;
};
template <typename T, typename Base>
requires (alignof(Base) == 1)
INLINE CreateExtendedResult<T, Base> create_extended (Buffer &buffer) {
    size_t padding = get_padding<T>(buffer.current_position() + sizeof_v<Base>);
    Buffer::Index<Base> base_idx = buffer.next_multi_byte<Base>(sizeof_v<Base> + padding + sizeof_v<T>);
    Buffer::Index<T> extended_idx = {static_cast<Buffer::index_t>(base_idx.value + padding + sizeof_v<Base>)};
    return {buffer.get_aligned(extended_idx), buffer.get_aligned(base_idx)};
}

template <typename T, typename Base, typename Next_T>
requires (alignof(Base) == 1)
INLINE CreateExtendedResult<T, Base> create_extended (Buffer &buffer) {
    Buffer::index_t pos = buffer.current_position();
    size_t padding_before = get_padding<T>(pos + sizeof_v<Base>);
    size_t size = sizeof_v<Base> + padding_before + sizeof_v<T>;
    size_t padding_after = get_padding<Next_T>(pos + size);
    Buffer::Index<Base> base_idx = buffer.next_multi_byte<Base>(size + padding_after);
    Buffer::Index<T> extended_idx = {static_cast<Buffer::index_t>(base_idx.value + padding_before + sizeof_v<Base>)};
    return {buffer.get_aligned(extended_idx), buffer.get_aligned(base_idx)};
}