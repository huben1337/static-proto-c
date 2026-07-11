#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <gsl/pointers>
#include <new>
#include <type_traits>
#include <utility>

namespace estd {

template<typename T>
struct vector32 {
    public:
        using value_type = T;
        using size_type = uint32_t;
        using difference_type = std::ptrdiff_t;

        using reference = T&;
        using const_reference = const T&;

        using pointer = T*;
        using const_pointer = const T*;

        using iterator = T*;
        using const_iterator = const T*;

    private:
        static constexpr bool use_c_style_allocation = std::is_trivially_move_constructible_v<T>
            && std::is_trivially_destructible_v<T>
            && alignof(T) <= alignof(max_align_t);

        gsl::owner<T*> _data = nullptr;
        uint32_t _capacity = 0;
        uint32_t _position = 0;

        [[nodiscard]] static constexpr uint32_t growth(uint32_t n)
        {
            return n < 8 ? 8 : n + (n / 2);
        }

        constexpr void reallocate(uint32_t new_capacity)
        {
            if constexpr (use_c_style_allocation) {
                _data = static_cast<gsl::owner<T*>>(std::realloc(_data, sizeof(T) * new_capacity));
                
                assert(_data != nullptr);
            } else {
                gsl::owner<T*> new_data;
                
                if constexpr (alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
                    new_data = static_cast<gsl::owner<T*>>(::operator new(sizeof(T) * new_capacity, std::nothrow));
                } else {
                    new_data = static_cast<gsl::owner<T*>>(::operator new(sizeof(T) * new_capacity, std::align_val_t{alignof(T)}, std::nothrow));
                }

                assert(new_data != nullptr);

                if constexpr (std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>) {
                    std::memcpy(new_data, _data, sizeof(T) * _position); //TODO Evalute performance with different copy approaches
                } else {
                    T* const end = _data + _position;
                    T* write = new_data;
                    for (T* read = _data; read != end; ++read, ++write) {
                        std::construct_at(write, std::move(*read));
                        if (!std::is_trivially_destructible_v<T>) {
                            std::destroy_at(read);
                        }
                    }
                }

                ::operator delete(_data);
                _data = new_data;
            }

            _capacity = new_capacity;
        }

        constexpr void destroy() {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                std::destroy_n(_data, _position);
            }
        
            if (use_c_style_allocation) {
                std::free(_data);
            } else {
                ::operator delete(_data);
            }
        }

        constexpr void reset() {
            _data = nullptr;
            _capacity = 0;
            _position = 0;
        }

    public:

        constexpr vector32() = default;

        constexpr explicit vector32(const uint32_t n)
        {
            resize(n);
        }

        constexpr vector32(vector32&& other)
            : _data(other._data),
            _capacity(other._capacity),
            _position(other._position)
        {
            other.reset();
        }

        ~vector32()
        {
            if (_data != nullptr) {
                destroy();
                reset();
            }
        }

        vector32& operator=(vector32&& other)
        {
            if (this == &other) {
                return *this;
            }

            if (_data != nullptr) {
                destroy();
            }

            _data = other._data;
            _capacity = other._capacity;
            _position = other._position;


            other.reset();
            return *this;
        }

        constexpr void swap(vector32& other)
        {
            std::swap(_data, other._data);
            std::swap(_capacity, other._capacity);
            std::swap(_position, other._position);
        }

        [[nodiscard]] constexpr const uint32_t& size() const
        {
            return _position;
        }

        [[nodiscard]] constexpr const uint32_t& capacity() const
        {
            return _capacity;
        }

        [[nodiscard]] constexpr bool empty() const
        {
            return _position == 0;
        }

        constexpr void shrink_to_fit()
        {
            if (_position != _capacity) {
                reallocate(_position);
            }
        }

        [[nodiscard]] constexpr T* data() { return _data; }
        [[nodiscard]] constexpr const T* data() const { return _data; }

        [[nodiscard]] constexpr T& operator[](uint32_t i)
        {
            assert(i < _position);

            return _data[i];
        }

        [[nodiscard]] constexpr const T& operator[](uint32_t i) const
        {
            assert(i < _position);

            return _data[i];
        }

        [[nodiscard]] constexpr T& at(uint32_t i)
        {
            assert(i < _position);

            return _data[i];
        }

        [[nodiscard]] constexpr const T& at(uint32_t i) const
        {
            assert(i < _position);

            return _data[i];
        }

        [[nodiscard]] constexpr T& front() { return _data[0]; }
        [[nodiscard]] constexpr const T& front() const { return _data[0]; }

        [[nodiscard]] constexpr T& back() { return _data[_position - 1]; }
        [[nodiscard]] constexpr const T& back() const { return _data[_position - 1]; }

        [[nodiscard]] constexpr iterator begin() { return _data; }
        [[nodiscard]] constexpr iterator end() { return _data + _position; }

        [[nodiscard]] constexpr const_iterator begin() const { return _data; }
        [[nodiscard]] constexpr const_iterator end() const { return _data + _position; }

        [[nodiscard]] constexpr const_iterator cbegin() const { return begin(); }
        [[nodiscard]] constexpr const_iterator cend() const { return end(); }

        void clear()
        {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                std::destroy_n(_data, _position);
            }

            _position = 0;
        }

        template<typename... Args>
        constexpr T& emplace_back(Args&&... args)
        {
            if (_position == _capacity) {
                reallocate(growth(_capacity));
            }

            T& result = *std::construct_at(
                _data + _position,
                std::forward<Args>(args)...
            );

            return result;
        }

        void push_back(const T& x)
        {
            emplace_back(x);
        }

        void push_back(T&& x)
        {
            emplace_back(std::move(x));
        }

        void pop_back()
        {
            --_position;

            if constexpr (!std::is_trivially_destructible_v<T>) {
                std::destroy_at(_data + _position);
            }
        }

        void reserve (uint32_t n) {
            if (n > _capacity) {
                reallocate(n);
            }
        }

    private:
        template<bool do_value_initialization>
        void resize(uint32_t n)
        {
            if (n <= _position) {
                
                if constexpr (!std::is_trivially_destructible_v<T>) {
                    std::destroy(_data + n, _data + _position);
                }
                
                _position = n;
                return;
            }

            if (n > _capacity) {
                reallocate(std::max(n, 8U));
            }


            if constexpr (do_value_initialization) {
                std::uninitialized_value_construct(_data + _position, _data + n);
            } else {
                if constexpr (!std::is_trivially_default_constructible_v<T>
                    || !std::is_trivially_copyable_v<T>) {
                    std::uninitialized_default_construct(_data + _position, _data + n);
                }
            }

            _position = n;
        }

    public:
        void resize(uint32_t n)
        {
            resize<true>(n);
        }

        void uninitialized_resize(uint32_t n)
        {
            resize<false>(n);
        }
    };
}