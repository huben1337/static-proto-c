#pragma once
#include <cstdint>
#include <limits>
#include <type_traits>
#include "base.cpp"
#include "helper_types.cpp"
#include "fatal_error.cpp"

template <typename U, U max = std::numeric_limits<U>::max()>
requires std::is_integral_v<U> && std::is_unsigned_v<U>
struct Memory {
    private:
    U capacity;
    U position = 0;
    uint8_t* memory;
    bool in_heap;

    public:
    INLINE Memory (U capacity) : capacity(capacity), in_heap(true) {
        memory = static_cast<uint8_t*>(std::malloc(capacity));
        if (!memory) {
            INTERNAL_ERROR("memory allocation failed\n");
        }
    }

    template <typename T, U N>
    requires (sizeof(T) * N <= max)
    INLINE Memory (T (&memory)[N]) : capacity(sizeof(T) * N), memory(reinterpret_cast<uint8_t*>(memory)), in_heap(false) {}

    INLINE Memory (uint8_t* memory, U capacity) : capacity(capacity), memory(memory), in_heap(false) {}

    using index_t = U;

    INLINE void clear () {
        position = 0;
    }


    INLINE void free () {
        if (in_heap) {
            std::free(memory);
        }
    }

    template <typename T>
    struct Index {
        U value;

        INLINE Index add (U offset) {
            return Index{value + offset};
        }

        INLINE Index sub (U offset) {
            return Index{value - offset};
        }
    };


    INLINE uint8_t* c_memory () {
        return memory;
    }

    INLINE constexpr U current_position () {
        return position;
    }

    template <typename T>
    INLINE T* get (Index<T> index) {
        return reinterpret_cast<T*>(memory + index.value);
    }

    template <typename T>
    INLINE T* get_aligned (Index<T> index) {
        return std::assume_aligned<alignof(T), T>(reinterpret_cast<T*>(memory + index.value));
    }

    template <typename T>
    INLINE T* get_next () {
        return get(get_next_idx<T>());
    }

    template <typename T>
    INLINE T* get_next_aligned () {
        return get_aligned(get_next_idx<T>());
    }


    template <typename T>
    INLINE Index<T> get_next_idx () {
        if constexpr (sizeof_v<T> == 0) {
            return Index<T>{position};
        } else if  constexpr (sizeof_v<T> == 1) {
            return get_next_single_byte<T>();
        } else {
            return get_next_multi_byte<T>(sizeof_v<T>);
        }
    }
    
    template <typename T>
    requires (sizeof_v<T> == 1)
    INLINE Index<T> get_next_single_byte () {
        if (position == capacity) {
            if (capacity >= (max / 2)) {
                INTERNAL_ERROR("memory overflow\n");
            }
            grow(capacity * 2);
        }
        return Index<T>{position++};
    }

    template <typename T, UnsignedIntegral Size>
    INLINE Index<T> get_next_multi_byte (Size size) {
        U next = position;
        position += size;
        if (position < next) {
            INTERNAL_ERROR("[Memory] position wrapped\n");
        }
        if (position >= capacity) {
            if (position >= (max / 2)) {
                INTERNAL_ERROR("memory overflow\n");
            }
            grow(position * 2);
        }
        return Index<T>{next};
    }

    private:
    INLINE void grow (U size) {
        if (in_heap) {
            memory = (uint8_t*) std::realloc(memory, size);
            // printf("reallocated memory in heap: to %d\n", capacity);
        } else {
            auto new_memory = (uint8_t*) std::malloc(size);
            memcpy(new_memory, memory, capacity);
            memory = new_memory;
            in_heap = true;
            // printf("reallocated memory from stack: to %d\n", capacity);
        }
        capacity = size;
        if (!memory) {
            INTERNAL_ERROR("memory allocation failed\n");
        }
    }

};

typedef Memory<uint32_t> Buffer;