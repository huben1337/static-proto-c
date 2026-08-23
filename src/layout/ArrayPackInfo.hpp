#pragma once

#include <cstdint>
#include <span>

#include "../core/SIZE.hpp"

namespace layout {

struct ReadOnlyArrayPackInfosSpan;
struct ArrayPackInfosSpan;

struct ArrayPackInfoBaseIdx;

struct ArrayPackInfoIdx {
    friend ArrayPackInfoBaseIdx;
    
    template <typename T>    
    friend struct ArrayPackInfosSpanBase;

private:
    uint16_t _value;

    constexpr explicit ArrayPackInfoIdx(uint16_t value) : _value(value) {}
public:

    [[nodiscard]] constexpr bool operator == (const ArrayPackInfoIdx& other) const {
        return _value == other._value;
    }

    [[nodiscard]] constexpr SIZE get_alignment () const {
        return SIZE::from_integral(_value);
    }

    [[nodiscard]] static constexpr ArrayPackInfoIdx invalid() {
        return ArrayPackInfoIdx{static_cast<uint16_t>(-1)};
    }
};

struct ArrayPackInfoBaseIdx {
    friend ArrayPackInfoIdx;

private:
    uint16_t _value;

    constexpr explicit ArrayPackInfoBaseIdx(uint16_t value) : _value(value) {}
public:
    // constexpr ArrayPackInfoBaseIdx() = default;

    constexpr ArrayPackInfoBaseIdx advance(this ArrayPackInfoBaseIdx& self) {
        const ArrayPackInfoBaseIdx before = self;
        self._value += (SIZE::MAX.ordinal() + 1);
        console.debug("Next pack info base idx #", before._value);
        return before;
    }

    [[nodiscard]] constexpr ArrayPackInfoIdx get_sub_idx(SIZE alignment) const {
        // if (_value == static_cast<uint16_t>(-1)) return ArrayPackInfoIdx::invalid();
        return ArrayPackInfoIdx{gsl::narrow_cast<uint16_t>(_value + alignment.ordinal())};
    }

    [[nodiscard]] static constexpr ArrayPackInfoBaseIdx invalid() {
        return ArrayPackInfoBaseIdx{static_cast<uint16_t>(-1) - SIZE::MAX.ordinal()};
    }

    [[nodiscard]] static constexpr ArrayPackInfoBaseIdx zero() {
        return ArrayPackInfoBaseIdx{0};
    }
};

struct ArrayPackInfo {
    uint64_t size = static_cast<uint64_t>(-1);
    ArrayPackInfoIdx parent_idx = ArrayPackInfoIdx::invalid();

    [[nodiscard]] constexpr const ArrayPackInfo& get_parent (const ReadOnlyArrayPackInfosSpan& pack_infos) const;
};


template <typename T>
struct ArrayPackInfosSpanBase {
    friend ArrayPackInfosSpan;
    friend ReadOnlyArrayPackInfosSpan;
private:
    std::span<T> _value;

public:
    constexpr explicit ArrayPackInfosSpanBase(std::span<T> value) : _value(value) {}


    [[nodiscard]] constexpr T& operator[](const ArrayPackInfoIdx i) const {
        return _value[i._value];
    }
};

struct ArrayPackInfosSpan
    : ArrayPackInfosSpanBase<ArrayPackInfo> {
    using ArrayPackInfosSpanBase::ArrayPackInfosSpanBase;
};

struct ReadOnlyArrayPackInfosSpan
    : ArrayPackInfosSpanBase<const ArrayPackInfo> {
    using ArrayPackInfosSpanBase::ArrayPackInfosSpanBase;

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr ReadOnlyArrayPackInfosSpan(const ArrayPackInfosSpan& other)
        : ArrayPackInfosSpanBase(
              std::span<const ArrayPackInfo>(other._value)) {}
};

[[nodiscard]] constexpr const ArrayPackInfo& ArrayPackInfo::get_parent (const ReadOnlyArrayPackInfosSpan& pack_infos) const {
    return pack_infos[parent_idx];
}


} // namespace layout
