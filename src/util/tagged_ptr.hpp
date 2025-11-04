#pragma once

#include <cstddef>
#include <cstdint>
#include <bit>
#include <concepts>
#include <type_traits>
#include <utility>

#include "../helper/ce.hpp"

template <size_t N>
requires (N <= 8)
struct ptr_tag {
    constexpr explicit ptr_tag (uint8_t value)
    : value(value)
    {
        if consteval {
            if (value >= ce::pow<2, N>) {
                std::unreachable();
            }
        }
    }
    
    using value_t = uint8_t;
    uint8_t value : N;

    static constexpr size_t bits = N;
};

struct bool_ptr_tag {
    constexpr explicit bool_ptr_tag (bool value)
    : value(value)
    {}

    using value_t = bool;
    bool value : 1;

    static constexpr size_t bits = 1;
};

namespace {

template <typename T>
concept ExposesBits = requires {
    { T::bits } -> std::convertible_to<const size_t>;
};

}

template <typename T, typename TagT, template <typename> typename PointerWrapper = std::type_identity_t>
struct tagged_ptr {

    static_assert(ExposesBits<TagT>, "TagT must extend ptr_tag and expose bits");
    static_assert(std::is_base_of_v<bool_ptr_tag, TagT> || std::is_base_of_v<ptr_tag<TagT::bits>, TagT>, "TagT must extend ptr_tag<TagT::bits> or bool_ptr_tag");
    static_assert(
        std::is_standard_layout_v<TagT>
        && sizeof(TagT) == 1
        && alignof(TagT) == 1,
        "TagT can add members or alter default tag alignment"
    );

    using wrapped_ptr_t = PointerWrapper<T*>;

    static constexpr uintptr_t AVAILABLE_TAG_BITS = ce::log2<alignof(T)>;
    static constexpr uintptr_t TAG_MASK = (uintptr_t{1} << AVAILABLE_TAG_BITS) - 1;
    static constexpr uintptr_t PTR_MASK = ~TAG_MASK;

    static_assert(AVAILABLE_TAG_BITS >= TagT::bits, "Can't fit Tag");
    static_assert(sizeof(wrapped_ptr_t) == sizeof(uintptr_t), "Wrapped pointer type must equal natural pointer size");

    constexpr tagged_ptr (wrapped_ptr_t ptr, TagT tag) : raw(std::bit_cast<uintptr_t>(ptr) | tag.value) {}
    constexpr tagged_ptr (std::nullptr_t /*unused*/, TagT tag) : raw(tag.value) {}

    private:
    uintptr_t raw;

    public:

    [[nodiscard]] constexpr PointerWrapper<T*> ptr () const {
        if consteval {
            if ((raw & PTR_MASK) == 0) {
                return PointerWrapper<T*>{nullptr};
            }
            std::unreachable();
        } else {
            return std::bit_cast<wrapped_ptr_t>(raw & PTR_MASK);
        }
    }

    constexpr void set_ptr (T* ptr) {
        raw = (raw & TAG_MASK) | std::bit_cast<uintptr_t>(ptr);
    }

    [[nodiscard]] constexpr TagT tag () const {
        return TagT{static_cast<TagT::value_t>(raw & TAG_MASK)};
    }
    constexpr void set_tag (TagT tag) {
        raw = (raw & PTR_MASK) | tag;
    }
};
