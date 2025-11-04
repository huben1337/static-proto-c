#pragma once

#include <type_traits>


namespace estd {
    /* Allow classes with private copy ctor to be copied when const. Add this class as a friend. */
    struct allow_const_copy {
        template <typename T>
        requires (!std::is_const_v<T> && !std::is_reference_v<T>)
        struct const_copy {
            private:
            T value;

            public:
            const_copy (T&) = delete;
            const_copy (T&&) = delete;
            const_copy (const T&&) = delete;
            constexpr explicit const_copy (const T& value) : value(value) {}

            [[nodiscard]] constexpr const T& get () const { return value; }
        };
    };

    template <typename T>
    // requires (!std::is_const_v<T>)
    using const_copy = allow_const_copy::const_copy<T>;
}

/* struct AA {
    int i = 0;
    constexpr AA () = default;
    private:
    constexpr AA (const AA&) = default;
    public:
    constexpr AA& operator = (const AA&) = delete;
    constexpr AA& operator = (AA&&) = default;
    constexpr AA (AA&&) = default;

    friend estd::allow_const_copy;
};

void test () {
    const auto aa = AA{};
    const int a = 0;
    auto bb = estd::const_copy{a};
    // auto cc = estd::const_copy<>
    bb.get().i++;
} */