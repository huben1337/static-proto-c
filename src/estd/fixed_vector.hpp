#pragma once

#include <gsl/pointers>
#include <initializer_list>
#include <memory>
#include <new>
#include <iostream>

namespace estd {

template <typename T>
class fixed_vector {
public:
    constexpr fixed_vector ()
    : data_(nullptr),
    size_(0)
    {}

    [[nodiscard]] static constexpr gsl::owner<T*> new_data_nothrow (size_t size) noexcept {
        gsl::owner<T*> data = static_cast<gsl::owner<T*>>(::operator new(sizeof(T) * size, std::align_val_t{alignof(T)}, std::nothrow));
        if (data == nullptr) {
            std::cout << "[fixed_vector::new_data_nothrow] Got nullptr when creating new data.\n";
            std::exit(1);
        }
        return data;
    }

    constexpr explicit fixed_vector (size_t size, const std::nothrow_t& /*unused*/) noexcept
    : data_(new_data_nothrow(size)),
    size_(size)
    {}

    constexpr explicit fixed_vector (size_t size) 
    : data_(static_cast<gsl::owner<T*>>(::operator new(sizeof(T) * size, std::align_val_t{alignof(T)}))),
    size_(size)
    {}

    constexpr fixed_vector (T* data, size_t size)
    : data_(data),
    size_(size)
    {}

    constexpr fixed_vector (std::initializer_list<T> il)
    : fixed_vector(il.size())
    {
        std::uninitialized_copy_n(il.begin(), il.size(), data_);
    }

    constexpr ~fixed_vector () {
        if (data_ == nullptr) return;
        destruct_held();
    }

    fixed_vector (const fixed_vector&) = delete;
    fixed_vector& operator= (const fixed_vector&) = delete;

    constexpr fixed_vector (fixed_vector&& other) noexcept
    : data_(other.data_),
    size_(other.size_)
    {
        other.data_ = nullptr;
    }

    constexpr fixed_vector& operator= (fixed_vector&& other) noexcept {
        if (*this == other) {
            return *this;
        }
        destruct_held();
        this->data_ = other.data_;
        this->size_ = other.size_;
        other.data_ = nullptr;
        return *this;
    }

    [[nodiscard]] constexpr T& operator[] (size_t index) {
        return data_[index];
    }

    [[nodiscard]] constexpr const T& operator[] (size_t index) const {
        return data_[index];
    }

    [[nodiscard]] constexpr size_t size () const { return size_; }
    [[nodiscard]] constexpr T* data () const { return data_; }

    [[nodiscard]] constexpr T* begin () const { return data_; }
    [[nodiscard]] constexpr T* end () const { return data_ + size_; }

private:
    gsl::owner<T*> data_;
    size_t size_;

    constexpr void destruct_held () {
        std::destroy(data_, data_ + size_);
        ::operator delete(data_);
    }
};

}