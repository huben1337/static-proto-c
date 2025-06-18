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
    static constexpr U max_position = std::numeric_limits<U>::max();
    static constexpr uint8_t grow_factor = 2;
    U capacity;
    U position = 0;
    uint8_t* memory;
    bool in_heap;

    INLINE constexpr Memory (const Memory& other) = default;
    INLINE constexpr Memory& operator = (const Memory& other) = default;

    friend class estd::const_copy_detail; // Give const_copy_detail access to private move constructor

    public:
    INLINE constexpr Memory (Memory&& other) = default;
    INLINE constexpr Memory& operator = (Memory&& other) = default;

    INLINE Memory (U capacity) : capacity(capacity), in_heap(true) {
        memory = static_cast<uint8_t*>(std::malloc(capacity));
        if (!memory) {
            INTERNAL_ERROR("memory allocation failed\n");
        };
    }

    template <typename T, U N>
    requires (sizeof(T) * N <= max_position)
    INLINE constexpr Memory (T (&memory)[N]) : capacity(sizeof(T) * N), memory(static_cast<uint8_t*>(static_cast<void*>(memory))), in_heap(false) {}

    INLINE constexpr Memory (void* memory, U capacity) : capacity(capacity), memory(static_cast<uint8_t*>(memory)), in_heap(false) {}

    using index_t = U;

    INLINE constexpr void clear () {
        position = 0;
    }

    INLINE void dispose () {
        if (in_heap) {
            std::free(memory);
            in_heap = false; // Prevent double free
        }
        capacity = 0;
        position = 0;
        memory = nullptr;
    }
    
    template <typename T>
    struct Index {
        INLINE constexpr Index () = default;
        INLINE constexpr Index (U value) : value(value) {}
        
        U value;

        INLINE constexpr Index add (U offset) const {
            return Index{value + offset};
        }

        INLINE constexpr Index sub (U offset) const {
            return Index{value - offset};
        }

        INLINE constexpr operator Index<const T>() const {
            return Index<const T>{value};
        }
    };

    template <typename T>
    struct Span {
        INLINE constexpr Span () = default;
        INLINE constexpr Span (Index<T> start_idx, Index<T> end_idx) : start_idx(start_idx), end_idx(end_idx) {}
        INLINE constexpr Span (Index<T> start_idx, U length) : start_idx(start_idx), end_idx(start_idx.add(length)) {}

        Index<T> start_idx;
        Index<T> end_idx;
        INLINE T* begin (const Memory& mem) const {
            return mem.get(start_idx);
        }
        INLINE T* end (const Memory& mem) const {
            return mem.get(end_idx);
        }

        INLINE constexpr U size () const {
            return end_idx.value - start_idx.value;
        }

        INLINE constexpr bool empty () const {
            return start_idx.value == end_idx.value;
        }
    };

    template <typename T>
    struct View {
        using length_t = fitting_uint<std::numeric_limits<U>::max() / sizeof(T)>;

        INLINE constexpr View () = default;
        INLINE constexpr View (Index<T> start_idx, length_t length) : start_idx(start_idx), length(length) {}
        INLINE constexpr View (Index<T> start_idx, Index<T> end_idx) : start_idx(start_idx), length((end_idx.value - start_idx.value) / sizeof(T)) {}
        Index<T> start_idx;
        length_t length;

        INLINE T* begin (const Memory& mem) const {
            return mem.get(start_idx);
        }
        INLINE T* end (const Memory& mem) const {
            return mem.get(start_idx.add(length));
        }

        INLINE constexpr U size () const {
            return length * sizeof(T);
        }

        INLINE constexpr bool empty () const {
            return length == 0;
        }
    };


    INLINE constexpr uint8_t* const& c_memory () const {
        return memory;
    }

    INLINE constexpr const U& current_position () const {
        return position;
    }

    INLINE constexpr const U& current_capacity () const {
        return capacity;
    }

    INLINE constexpr void go_back (U amount) {
        position -= amount;
    }

    INLINE constexpr void go_to (U position) {
        if (position > this->position) {
            INTERNAL_ERROR("[Memory::go_to] cant go forward\n");
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
    INLINE constexpr T* get (Index<T> index) const {
        return reinterpret_cast<T*>(memory + index.value);
    }

    template <typename T>
    INLINE constexpr T* get_aligned (Index<T> index) const {
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

    INLINE constexpr void close () {
        in_heap = false;
        capacity = 0;
        position = 0;
        memory = nullptr;
    }
};

typedef Memory<uint32_t> Buffer;

typedef Buffer::View<char> BufferStringView;

#define MEMORY_INIT_STACK(SIZE) alloca(SIZE), SIZE