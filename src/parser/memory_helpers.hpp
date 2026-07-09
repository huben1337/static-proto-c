#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "../container/memory.hpp"

namespace lexer {

template <typename T, typename At>
requires (alignof(At) == 1)
[[nodiscard, gnu::always_inline]] constexpr T& get_padded (At* at) {
    static_assert(!std::is_const_v<At> || std::is_const_v<T>);
    return *std::assume_aligned<alignof(T)>(reinterpret_cast<T*>(math::next_multiple<uintptr_t, alignof(T)>(estd::ptr_as_integral(at))));
}

template <typename Header, typename T>
struct HeaderDataBufferIndexPair {
    Buffer::Index<Header> header;
    Buffer::Index<T> extended;
};

template <typename Header, typename T>
requires (alignof(Header) == 1)
[[nodiscard]] inline HeaderDataBufferIndexPair<Header, T> create_with_header (Buffer &buffer) {
    Buffer::Index<Header> header_idx = buffer.next<Header>();
    Buffer::Index<T> extended_idx = buffer.next<T>();
    return {header_idx, extended_idx};
}

template <typename Header, typename T>
requires (alignof(Header) == 1)
[[nodiscard]] inline Buffer::Index<Header> create_with_header (Buffer &buffer, Header&& header, T&& extended) {
    const HeaderDataBufferIndexPair<Header, T> created = create_with_header<std::remove_cvref_t<Header>, std::remove_cvref_t<T>>(buffer);
    buffer.get(created.header) = std::forward<Header>(header);
    buffer.get(created.extended) = std::forward<T>(extended);
    return created.header;
}


}