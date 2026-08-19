#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <gsl/pointers>
#include <new>
#include <type_traits>
#include <utility>

#include "./basic_contiguous_iterator.hpp"

namespace estd {

template<typename T>
struct array {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;

    using reference = T&;
    using const_reference = const T&;

    using pointer = T*;
    using const_pointer = const T*;

    using iterator = basic_contiguous_iterator<T>;
    using const_iterator = basic_contiguous_iterator<const T>;

private:
    gsl::owner<T*> _data;
    size_type _size;

    [[nodiscard]]
    static constexpr gsl::owner<T*> allocate_handled(const size_type size) {
        if (size == 0) return nullptr;

        if constexpr (alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            return static_cast<gsl::owner<T*>>(::operator new(sizeof(T) * size));
        } else {
            return static_cast<gsl::owner<T*>>(::operator new(sizeof(T) * size, std::align_val_t{alignof(T)}));
        }
    }

    constexpr void initialize_data_from_other(const array& other) {
        if constexpr (std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>) {
            std::memcpy(_data, other._data, sizeof(T) * _size);
        } else {
            std::uninitialized_copy_n(other._data, _size, _data);
        }
    }

    constexpr void destroy() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            std::destroy_n(_data, _size);
        }
    
        if constexpr (alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            ::operator delete(_data);
        } else {
            ::operator delete(_data, std::align_val_t{alignof(T)});
        }
    }

    constexpr void reset() {
        _data = nullptr;
        _size = 0;
    }

public:

    constexpr array() : _data(nullptr), _size(0) {}

    constexpr explicit array(const size_type n) :
        _data(allocate_handled(n)),
        _size(n)
    {
        if constexpr (!std::is_trivially_default_constructible_v<T>
            || !std::is_trivially_copyable_v<T>) {
            std::uninitialized_default_construct_n(_data, _size);
        }
    }

    constexpr array(const size_type n, const value_type& value) :
        _data(allocate_handled(n)),
        _size(n)
    {
        std::uninitialized_fill_n(_data, _size, value);
    }

    constexpr array(const array& other) :
        _data(allocate_handled(other._size)),
        _size(other._size)
    {
        initialize_data_from_other(other);
    }

    constexpr array(array&& other) :
        _data(other._data),
        _size(other._size)
    {
        other.reset();
    }

    constexpr ~array() {
        if (_data != nullptr) {
            destroy();
            reset();
        }
    }

    constexpr array& operator=(const array& other) {
        if (this == &other) return *this;

        if (_data != nullptr) {
            destroy();
        }

        _data = allocate_handled(other._size);
        _size = other._size;

        initialize_data_from_other(other);

        return *this;
    }

    constexpr array& operator=(array&& other) {
        if (this == &other) return *this;

        if (_data != nullptr) {
            destroy();
        }

        _data = other._data;
        _size = other._size;


        other.reset();
        return *this;
    }

    constexpr void swap(array& other) {
        std::swap(_data, other._data);
        std::swap(_size, other._size);
    }

    [[nodiscard]]
    constexpr const size_type& size() const {
        return _size;
    }


    [[nodiscard]]
    constexpr bool empty() const{
        return _size == 0;
    }

    [[nodiscard]] constexpr T* data() { return _data; }
    [[nodiscard]] constexpr const T* data() const { return _data; }

    [[nodiscard]]
    constexpr T& at(size_type i) {
        assert(i < _size);

        return _data[i];
    }

    [[nodiscard]]
    constexpr const T& at(size_type i) const
    {
        assert(i < _size);

        return _data[i];
    }

    [[nodiscard]] constexpr T& operator[](size_type i) { return at(i); }
    [[nodiscard]] constexpr const T& operator[](size_type i) const { return at(i); }

    [[nodiscard]] constexpr T& front() { return at(0); }
    [[nodiscard]] constexpr const T& front() const { return at(0); }

    [[nodiscard]] constexpr T& back() {
        assert(_size > 0);
        return _data[_size - 1];
    }

    [[nodiscard]] constexpr const T& back() const {
        return at(_size - 1);
    }

    [[nodiscard]]
    constexpr iterator begin() {
        return iterator::at_range_begin(_data, _data + _size);
    }

    [[nodiscard]]
    constexpr iterator end() {
        return iterator::at_range_end(_data, _data + _size);
    }

    [[nodiscard]]
    constexpr const_iterator begin() const {
        return const_iterator::at_range_begin(_data, _data + _size);
    }

    [[nodiscard]]
    constexpr const_iterator end() const {
        return const_iterator::at_range_end(_data, _data + _size);
    }

    [[nodiscard]] constexpr const_iterator cbegin() const { return begin(); }
    [[nodiscard]] constexpr const_iterator cend() const { return end(); }
};

} // namespace estd

consteval int a () {
    const estd::array<int> arr (1);

    for ([[maybe_unused]] auto&& n : arr) {
    
    }

    return arr.back();
}