#include "base.cpp"
#include "constexpr_helpers.cpp"
#include <concepts>
#include <cstddef>


template <std::unsigned_integral T, size_t start, size_t end>
requires (start < end)
constexpr T ones_at = ((T{1} << (end - start)) - 1) << start;

template <std::unsigned_integral T, size_t start, size_t end>
requires (start < end)
constexpr T zeros_at = ~ones_at<T, start, end>;

template <typename T, std::integral TagT>
struct tagged_ptr {
    static constexpr uintptr_t TAG_BITS = uint_log2<alignof(T)>; // assumnes we adress memory in bytes
    static constexpr uintptr_t TAG_MASK = (1ULL << TAG_BITS) - 1;
    static constexpr uintptr_t PTR_MASK = ~TAG_MASK;
    static_assert(TAG_BITS + 7 >= sizeof(TagT) * 8, "Can't fit Tag");

    constexpr tagged_ptr (T* ptr, TagT tag) : raw(reinterpret_cast<uintptr_t>(ptr) | tag) {}

    private:
    uintptr_t raw;

    public:

    INLINE constexpr T* ptr () const {
        return reinterpret_cast<T*>(raw & PTR_MASK);
    }
    INLINE constexpr void set_ptr (T* ptr) {
        raw = (raw & TAG_MASK) | reinterpret_cast<uintptr_t>(ptr);
    }

    INLINE constexpr TagT tag () const {
        return raw & TAG_MASK;
    }
    INLINE constexpr void set_tag (TagT tag) {
        raw = (raw & PTR_MASK) | tag;
    }
};
