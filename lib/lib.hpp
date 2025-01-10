#include <cstdint>

template <typename T>
static inline T* add_offset(size_t base, size_t offset) {
    return reinterpret_cast<T*>(base + offset);
}