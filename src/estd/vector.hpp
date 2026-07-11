#pragma once

#include <utility>
#include <iterator>
#include <vector>

namespace estd {
   
namespace detail {

template <typename T>
constexpr void move_append (std::vector<T>& dest, const T& src) {
    dest.push_back(src);
}

template <typename T>
constexpr void move_append (std::vector<T>& dest, T&& src) {
    dest.push_back(std::forward<T>(src));
}

template <typename T>
constexpr void move_append_vector (std::vector<T>& dest, const std::vector<T>& src) {
    dest.insert(dest.end(),
        std::make_move_iterator(src.begin()),
        std::make_move_iterator(src.end())
    );
}

template <typename T>
constexpr void move_append (std::vector<T>& dest, std::vector<T>&& src) {
    move_append_vector(dest, std::move(src));
}

template <typename T>
constexpr void move_append (std::vector<T>& dest, std::vector<T>& src) {
    move_append_vector(dest, src);
    src.clear();
}

template <typename T>
[[nodiscard]] constexpr size_t element_count (const T& /*unused*/) {
    return 1;
}

template <typename T>
[[nodiscard]] constexpr size_t element_count (const std::vector<T>& src) {
    return src.size();
}

} // namespace detail

template <typename T, typename... SrcTs>
constexpr void move_append (std::vector<T>& dest, size_t additionaly_reserved, SrcTs&&... srcs) {
    dest.reserve(dest.size() + (detail::element_count(srcs) + ...) + additionaly_reserved);
    (detail::move_append(dest, std::forward<SrcTs>(srcs)), ...);
}

} // namespace estd