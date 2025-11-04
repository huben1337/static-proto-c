template <typename T, T... Values>
struct PackHolder {
    template <T... OtherValues>
    consteval auto operator + (PackHolder<T, OtherValues...> /*unused*/) const {
        return PackHolder<T, Values..., OtherValues...>{};
    }
};