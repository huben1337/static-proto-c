#pragma once

#include <cstdint>

namespace estd {

struct u48_u16_pair {
    protected:
    static constexpr uint64_t U48_MASK = (uint64_t{1} << 48) - 1;
    static constexpr uint64_t U16_MASK = ~U48_MASK;

    public:
    uint64_t data;

    constexpr u48_u16_pair () = default;
    constexpr explicit u48_u16_pair(uint64_t data) : data(data) {}
    constexpr u48_u16_pair (uint64_t u48, uint16_t u16)
    : data(u48 | uint64_t{u16} << 48)
    {}

    [[nodiscard]] constexpr uint16_t get_u16 () const {
        return data >> 48;
    }

    constexpr void set_u16 (uint16_t value) {
        data = (data & U48_MASK) | uint64_t{value} << 48;
    }

    [[nodiscard]] constexpr uint64_t get_u48 () const {
        return data & U48_MASK;
    }

    constexpr void set_u48 (uint64_t value) {
        data = (data & U16_MASK) | value;
    }

    [[nodiscard]] constexpr bool operator == (const u48_u16_pair& other) const {
        return data == other.data;
    }
};

namespace {

struct _fake_integral { _fake_integral () = delete; };

}

struct int48_t : _fake_integral {};
struct uint48_t : _fake_integral {};

namespace {

template <typename T, typename U>
struct _integral_pair;

template <>
struct _integral_pair<uint48_t, uint16_t> {
    using type = u48_u16_pair;
};

}

template <typename T, typename U>
using integral_pair = _integral_pair<T, U>::type;

}