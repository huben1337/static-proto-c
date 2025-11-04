#pragma once

#include <cstdint>
#include <type_traits>

template<typename T>
concept u64or32 = std::is_same_v<T, uint64_t> || std::is_same_v<T, uint32_t>;
template<typename T>
concept s64or32 = std::is_same_v<T, int64_t> || std::is_same_v<T, int32_t>;
