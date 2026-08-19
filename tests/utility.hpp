#pragma once

#include <boost/ut.hpp>

namespace test {

struct LifetimeStats {
    int default_ctor{};
    int value_ctor{};
    int copy_ctor{};
    int move_ctor{};
    int copy_assign{};
    int move_assign{};
    int dtor{};
    int alive{};
};

struct Tracked {
    [[gnu::visibility("default")]] inline static LifetimeStats stats {};

    int value{-1};

    static void reset() { stats = {}; }

    Tracked() {
        ++stats.default_ctor;
        ++stats.alive;
    }

    explicit Tracked(int v) : value(v) {
        ++stats.value_ctor;
        ++stats.alive;
    }

    Tracked(const Tracked& other) : value(other.value) {
        ++stats.copy_ctor;
        ++stats.alive;
    }

    Tracked(Tracked&& other) noexcept : value(other.value) {
        other.value = -999;
        ++stats.move_ctor;
        ++stats.alive;
    }

    Tracked& operator=(const Tracked& other) {
        value = other.value;
        ++stats.copy_assign;
        return *this;
    }

    Tracked& operator=(Tracked&& other) noexcept {
        value = other.value;
        other.value = -999;
        ++stats.move_assign;
        return *this;
    }

    ~Tracked() {
        ++stats.dtor;
        --stats.alive;
    }

    constexpr bool operator==(const Tracked& other) const {
        return value == other.value;
    }
};

struct NonCopyable {
    int value{};

    NonCopyable() = default;
    explicit NonCopyable(int v) : value(v) {}

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) noexcept = default;
    NonCopyable& operator=(NonCopyable&&) noexcept = default;
};

struct NonMovable {
    int value{};

    NonMovable() = default;
    explicit NonMovable(int v) : value(v) {}

    NonMovable(const NonMovable&) = default;
    NonMovable& operator=(const NonMovable&) = default;
    NonMovable(NonMovable&&) = delete;
    NonMovable& operator=(NonMovable&&) = delete;
};

struct TrivialValue {
    int value{};

    [[nodiscard]] constexpr bool operator==(const TrivialValue& other) const {
        return value == other.value;
    }
};

struct sl_guard {

    friend inline void expect_stats(
        int default_ctor,
        int value_ctor,
        int copy_ctor,
        int move_ctor,
        int copy_assign,
        int move_assign,
        int dtor,
        int alive,
        const sl_guard& /*unused*/,
        const boost::ut::reflection::source_location& sl
    );
    
private:
    constexpr sl_guard() = default;
};

inline void expect_stats(
    const int default_ctor,
    const int value_ctor,
    const int copy_ctor,
    const int move_ctor,
    const int copy_assign,
    const int move_assign,
    const int dtor,
    const int alive,
    const sl_guard&  /*unused*/ = {},
    const boost::ut::reflection::source_location& sl = boost::ut::reflection::source_location::current()
)  {
    boost::ut::expect(boost::ut::eq(Tracked::stats.default_ctor, default_ctor), sl);
    boost::ut::expect(boost::ut::eq(Tracked::stats.value_ctor, value_ctor), sl);
    boost::ut::expect(boost::ut::eq(Tracked::stats.copy_ctor, copy_ctor), sl);
    boost::ut::expect(boost::ut::eq(Tracked::stats.move_ctor, move_ctor), sl);
    boost::ut::expect(boost::ut::eq(Tracked::stats.copy_assign, copy_assign), sl);
    boost::ut::expect(boost::ut::eq(Tracked::stats.move_assign, move_assign), sl);
    boost::ut::expect(boost::ut::eq(Tracked::stats.dtor, dtor), sl);
    boost::ut::expect(boost::ut::eq(Tracked::stats.alive, alive), sl);
}


}