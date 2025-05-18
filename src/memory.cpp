#pragma once
#include <cstdint>
#include <cstring>
#include <concepts>
#include <limits>
#include <memory>
#include "base.cpp"
#include "helper_types.cpp"
#include "fatal_error.cpp"
#include "logger.cpp"

template <std::unsigned_integral U>
struct Memory {
    private:
    constexpr static U max_position = std::numeric_limits<U>::max();
    constexpr static uint8_t grow_factor = 2;
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
    requires (sizeof(T) * N <= max_position)
    INLINE Memory (T (&memory)[N]) : capacity(sizeof(T) * N), memory(reinterpret_cast<uint8_t*>(memory)), in_heap(false) {}

    INLINE Memory (uint8_t* memory, U capacity) : capacity(capacity), memory(memory), in_heap(false) {}

    using index_t = U;

    INLINE void clear () {
        position = 0;
    }

    INLINE void dispose () {
        if (in_heap) {
            std::free(memory);
            in_heap = false; // Prevent double free
        }
    }

    template <typename T>
    struct Index {
        U value;

        //constexpr Index (U value) : value(value) {}

        INLINE Index add (U offset) const {
            return Index{value + offset};
        }

        INLINE Index sub (U offset) const {
            return Index{value - offset};
        }

        INLINE operator Index<const T>() const {
            return Index<const T>{std::move(value)};
        }
    };

    template <typename T>
    struct Span {
        Span () {}
        Span (Index<T> start_idx, Index<T> end_idx) : start_idx(start_idx), end_idx(end_idx) {}
        Span (Index<T> start_idx, U length) : start_idx(start_idx), end_idx(start_idx.add(length)) {}

        Index<T> start_idx;
        Index<T> end_idx;
        INLINE T* begin (Memory mem) {
            return mem.get(start_idx);
        }
        INLINE T* end (Memory mem) {
            return mem.get(end_idx);
        }

        INLINE bool empty () {
            return start_idx.value == end_idx.value;
        }
    };

    template <typename T>
    struct View {
        View () {}
        View (Index<T> start_idx, U length) : start_idx(start_idx), length(length) {}
        View (Index<T> start_idx, Index<T> end_idx) : start_idx(start_idx), length((end_idx.value - start_idx.value) / sizeof(T)) {}
        Index<T> start_idx;
        U length;

        INLINE T* begin (Memory mem) const {
            return mem.get(start_idx);
        }
        INLINE T* end (Memory mem) const {
            return mem.get(start_idx.add(length));
        }

        INLINE bool empty () const {
            return length == 0;
        }
    };


    INLINE uint8_t* c_memory () const {
        return memory;
    }

    INLINE constexpr U current_position () const {
        return position;
    }

    INLINE constexpr U current_capacity () const {
        return capacity;
    }

    INLINE constexpr void go_back (U amount) {
        position -= amount;
    }

    INLINE constexpr void go_to (U position) {
        if (position > this->position) {
            logger::error("[Memory::go_to] cant go forward\n");
            exit(1);
        }
        this->position = position;
    }

    INLINE constexpr void go_to_unsafe (U position) {
        this->position = position;
    }

    template <typename T>
    INLINE constexpr Index<T> position_idx () const {
        return {position};
    }

    template <typename T>
    INLINE T* get (Index<T> index) const {
        return reinterpret_cast<T*>(memory + index.value);
    }

    template <typename T>
    INLINE T* get_aligned (Index<T> index) const {
        return std::assume_aligned<alignof(T), T>(reinterpret_cast<T*>(memory + index.value));
    }

    template <typename T>
    INLINE T* get_next () {
        return get(next_idx<T>());
    }

    template <typename T>
    INLINE T* get_next_aligned () {
        return get_aligned(next_idx<T>());
    }

    template <typename T>
    INLINE Index<T> next_idx () {
        if constexpr (sizeof_v<T> == 0) {
            return Index<T>{position};
        } else if  constexpr (sizeof_v<T> == 1) {
            return next_single_byte<T>();
        } else {
            return next_multi_byte<T>(sizeof_v<T>);
        }
    }

    template <typename T>
    INLINE T* get_next_single_byte () {
        return get(next_single_byte<T>());
    }

    template <typename T>
    INLINE T* get_next_single_byte_aligned () {
        return get_aligned(next_single_byte<T>());
    }
    
    template <typename T>
    INLINE Index<T> next_single_byte () {
        if (position == capacity) {
            grow();
        }
        return Index<T>{position++};
    }

    template <typename T, std::unsigned_integral SizeT>
    INLINE T* get_next_multi_byte (SizeT size) {
        return get(next_multi_byte<T>(size));
    }

    template <typename T, std::unsigned_integral SizeT>
    INLINE T* get_next_multi_byte_aligned (SizeT size) {
        return get_aligned(next_multi_byte<T>(size));
    }

    template <typename T, std::unsigned_integral SizeT>
    INLINE Index<T> next_multi_byte (SizeT size) {
        U next = position;
        position += size;
        if (position < next) {
            logger::error("[Memory::next_multi_byte] position overflow\n");
            exit(1);
        }
        if (position >= capacity) {
            grow();
        }
        return Index<T>{next};
    }

    private:
    INLINE void grow () {
        U new_capacity;;
        if (position >= (max_position / grow_factor)) {
            logger::warn("[Memory::grow] capped growth\n");
            new_capacity = max_position;
        } else {
            new_capacity = position * grow_factor;
        }
        if (in_heap) {
            memory = reinterpret_cast<uint8_t*>(std::realloc(memory, new_capacity));
            // printf("reallocated memory in heap: to %d\n", capacity);
        } else {
            uint8_t* heap_memory = reinterpret_cast<uint8_t*>(std::malloc(new_capacity));
            std::memcpy(heap_memory, memory, capacity);
            memory = heap_memory;
            in_heap = true;
            // printf("reallocated memory from stack: to %d\n", capacity);
        }
        capacity = new_capacity;
    }

};

typedef Memory<uint32_t> Buffer;

typedef Buffer::View<char> BufferStringView;