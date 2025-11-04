#pragma once

namespace estd {

template <typename T>
struct bound {
    private:
    T min;
    T max;

    public:
    constexpr bound (T min, T max) : min(min), max(max) {}

    constexpr bool operator () (T result) const {
        return result >= min && result <= max;
    }

    struct lower {
        private:
        T min;

        public:
        constexpr explicit lower (T min) : min(min) {}

        constexpr bool operator () (T result) const {
            return result >= min;
        }
    };

    struct upper {
        private:
        T max;

        public:
        constexpr explicit upper (T max) : max(max) {}

        constexpr bool operator () (T result) const {
            return result <= max;
        }
    };
};

} // namespace estd