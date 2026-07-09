#pragma once

#include <fcntl.h>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <gsl/util>
#include <limits>
#include <memory>
#include <gsl/pointers>
#include <ranges>
#include <span>
#include <utility>

#include "../util/logger.hpp"
#include "../util/tagged_ptr.hpp"
#include "../estd/type_traits.hpp"
#include "../math/multiples.hpp"

template <typename, std::unsigned_integral>
struct MemoryBase;

template <std::unsigned_integral, typename>
struct Memory;

template <std::unsigned_integral U>
struct MemoryTypes {
    friend struct StructDefintionEligibleBase;
    using index_t = U;

    template <typename T>
    struct Index {
        template <typename>
        friend struct Index;

        template <typename, std::unsigned_integral>
        friend struct ::MemoryBase;

        template <std::unsigned_integral, typename>
        friend struct ::Memory;

        U value = ~U{0};

        constexpr Index () = delete;
    private:
        constexpr explicit Index (U value) : value(value) {}

    public:
        // NOLINTNEXTLINE(google-explicit-constructor)
        [[nodiscard]] constexpr operator Index<const T>() const {
            return Index<const T>{value};
        }
    };

    template <typename T>
    struct Span {
        template <typename>
        friend struct Span;

        template <typename, std::unsigned_integral>
        friend struct ::MemoryBase;

        template <std::unsigned_integral, typename>
        friend struct ::Memory;

    private:
        U start_idx = 0;
        U end_idx = 0;
        
    public:
        constexpr Span () = default;

    private:
        constexpr Span (U start_idx, U end_idx) : start_idx(start_idx), end_idx(end_idx) {}
        
    public:
        [[nodiscard]] static consteval Span invalid() {
            return {~U{0}, ~U{0}};
        }

        [[nodiscard]] constexpr U size () const {
            return end_idx - start_idx;
        }

        [[nodiscard]] constexpr bool empty () const {
            return start_idx == end_idx;
        }

        // NOLINTNEXTLINE(google-explicit-constructor)
        [[nodiscard]] constexpr operator Span<const T>() const {
            return Span<const T>{start_idx, end_idx};
        }
    };

    struct ViewFrom;

    template <typename T>
    struct View {
        template <typename>
        friend struct View;

        template <typename, std::unsigned_integral>
        friend struct ::MemoryBase;

        template <std::unsigned_integral, typename>
        friend struct ::Memory;

        friend struct ViewFrom;
        
        using length_t = estd::fitting_uint_t<std::numeric_limits<U>::max() / sizeof(T)>;

        U start_idx = 0;
        length_t length = 0;
        
        constexpr View () = default;
    private:
        constexpr View (U start_idx, length_t length) : start_idx(start_idx), length(length) {}

    public:
        [[nodiscard]] static consteval View invalid() {
            return {~U(0), ~length_t{0}};
        }

        [[nodiscard]] constexpr U size () const {
            return length * sizeof(T);
        }

        [[nodiscard]] constexpr bool empty () const {
            return length == 0;
        }

        // NOLINTNEXTLINE(google-explicit-constructor)
        [[nodiscard]] constexpr operator View<const T>() const {
            return View<const T>{start_idx, length};
        }
    };
};

template <typename Derived, std::unsigned_integral U>
struct MemoryBase : MemoryTypes<U> {
    friend Derived;

private:
    using Base = MemoryTypes<U>;

    constexpr MemoryBase() = default;

    template <typename T>
    [[nodiscard]] constexpr T* _get (this const Derived& self, const U index) {
        return std::assume_aligned<alignof(T), T>(reinterpret_cast<T*>(self.data() + index));
    }

public:
    template <typename T>
    [[nodiscard]] constexpr T& get (this const Derived& self, const Base::template Index<T> index) {
        return *self.template _get<T>(index.value);
    }

    template <typename T>
    [[nodiscard]] constexpr std::ranges::subrange<T*> get (this const Derived& self, const Base::template Span<T> span) {
        return {self.template _get<T>(span.start_idx), self.template _get<T>(span.end_idx)};
    }

    template <typename T>
    [[nodiscard]] constexpr std::span<T> get (this const Derived& self, const Base::template View<T> view) {
        return {self.template _get<T>(view.start_idx), view.length};
    }
};

template <std::unsigned_integral U>
struct ReadOnlyMemoryView;

template<typename Derived, std::unsigned_integral U>
struct MemoryWriteProvider {
    friend Derived;

private:
    constexpr MemoryWriteProvider() = default;

public:
    template <typename T>
    struct Write {
        friend MemoryWriteProvider;

    private:
        Derived& _mem;
        #ifndef NDEBUG
        T* _begin;
        #endif
        T* _dst;
    public:
        constexpr Write(const Write&) = delete;
        constexpr Write(Write&&) = delete;

        constexpr Write& operator=(const Write&) = delete;
        constexpr Write& operator=(Write&&) = delete;

        constexpr ~Write() {
            if (_dst == nullptr) return;
            assert(_dst >= _begin);
            assert(_dst <= reinterpret_cast<T*>(_mem.data()) + _mem._position);
            _mem._position = static_cast<U>(_dst - reinterpret_cast<T*>(_mem.data()));
            _dst = nullptr;
        }

    private:
        constexpr explicit Write(Derived& mem, T* const begin)
        : _mem(mem),
        #ifndef NDEBUG
        _begin(begin),
        #endif
        _dst(begin) {}

    public:
        [[nodiscard]] constexpr T*& dst () {
            return _dst;
        }

        template <typename Writer, typename... Args>
        constexpr void with (Writer&& writer, Args&&... args) {
            _dst = std::forward<Writer>(writer)(_dst, std::forward<Args>(args)...);
        }
    };

private:
    template <typename T>
    requires (alignof(T) == 1)
    constexpr Write<T> write (this Derived& self, const U size) {
        const U start = self.allocate(size);
        T* const begin = reinterpret_cast<T*>(self.data()) + start;
        return Write<T>{
            self,
            begin
        };
    }
    
    template <typename T, typename Writer, typename... Args>
    requires (alignof(T) == 1)
    constexpr void write_with (this Derived& self, const U size, Writer&& writer, Args&&... args) {
        const U start = self.allocate(size);
        T* const begin = reinterpret_cast<T*>(self.data()) + start;
        T* const end = std::forward<Writer>(writer)(begin, std::forward<Args>(args)...);
        assert(end >= begin);
        assert(end <= reinterpret_cast<T*>(self.data()) + self._position);
        self._position = static_cast<U>(end - reinterpret_cast<T*>(self.data()));
    }
};

template <std::unsigned_integral U, typename AllocatedT>
struct Memory : MemoryBase<Memory<U, AllocatedT>, U>, MemoryWriteProvider<Memory<U, AllocatedT>, U> {
    using Base = MemoryBase<Memory<U, AllocatedT>, U>;
    using WritingProvider = MemoryWriteProvider<Memory<U, AllocatedT>, U>;

    friend struct ReadOnlyMemoryView<U>;
    friend WritingProvider;

    using backing_t = AllocatedT;

    template <typename T>
    using Index = Base::template Index<T>;

    template <typename T>
    using Span = Base::template Span<T>;

    template <typename T>
    using View = Base::template View<T>;

private:
    using tagged_data_ptr_t = tagged_ptr<AllocatedT, bool_ptr_tag, gsl::owner>;

    static constexpr U max_position = std::numeric_limits<U>::max();
    static constexpr uint8_t grow_factor = 2;
    static constexpr size_t alignment = std::max(alignof(max_align_t), alignof(AllocatedT));
    static constexpr tagged_data_ptr_t tagged_data_nullptr {nullptr, bool_ptr_tag{false}};

    U _capacity;
    U _position = 0;


    tagged_data_ptr_t _data;

    [[nodiscard]] static constexpr gsl::owner<AllocatedT*> alloc_handled (U capacity) {
        gsl::owner<AllocatedT*> allocated;
        if constexpr (alignment > alignof(max_align_t)) {
            allocated = static_cast<gsl::owner<AllocatedT*>>(std::aligned_alloc(alignment, capacity));
        } else {
            allocated = static_cast<gsl::owner<AllocatedT*>>(std::malloc(capacity));
        }

        assert(allocated != nullptr);

        return allocated;
    }

public:
    constexpr explicit Memory (U capacity) : _capacity(capacity), _data(alloc_handled(capacity), bool_ptr_tag{true}) {}

    template <U N>
    constexpr explicit Memory (AllocatedT (&data)[N]) : _capacity(sizeof(AllocatedT) * N), _data(data, bool_ptr_tag{false}) {
        static_assert(sizeof(AllocatedT) * N <= max_position, "capacity overflow");
    }

private:
    constexpr Memory (AllocatedT* data, U capacity) : _capacity(capacity), _data(data, bool_ptr_tag{false}) {}

public:
    template<estd::discouraged_annotation>
    static constexpr Memory from_stack (AllocatedT* data, U capacity) {
        return Memory{data, capacity};
    }

    constexpr Memory (const Memory&) = delete;
    constexpr Memory& operator = (const Memory&) = delete;

    constexpr Memory (Memory&& other) : _capacity(other._capacity), _position(other._position), _data(other._data) {
        other.reset();
    }
    constexpr Memory& operator = (Memory&& other) {
        if (this == &other) return *this;

        if (in_heap()) {
            std::free(_data.ptr());
        }

        _data = other._data;
        _capacity = other._capacity;
        _position = other._position;

        other.reset();
        return *this;
    }


    constexpr ~Memory () {
        if (in_heap()) {
            std::free(_data.ptr());
        }
        reset();
    }

    [[nodiscard]] constexpr std::byte* data () const {
        return reinterpret_cast<std::byte*>(_data.ptr());
    }

    [[nodiscard]] constexpr const U& position () const {
        return _position;
    }

    [[nodiscard]] constexpr const U& capacity () const {
        return _capacity;
    }

private:
    [[nodiscard]] constexpr bool in_heap () const {
        return _data.tag().value;
    }

    constexpr void reset () {
        _data = tagged_data_nullptr;
        
        #ifndef NDEBUG

        _capacity = 0;
        _position = 0;

        #endif
    }

    constexpr U allocate (U size) {
        U next = _position;
        _position += size;
        assert(_position >= next);
        if (_position >= _capacity) {
            grow();
        }
        return next;
    }

    constexpr void grow () {
        U new_capacity;
        if (_position >= (max_position / grow_factor)) {
            console.warn("[Memory::grow] capped growth.");
            new_capacity = max_position;
        } else {
            new_capacity = _position * grow_factor;
        }
        if constexpr (alignment <= alignof(max_align_t)) {
            if (in_heap()) {
                gsl::owner<AllocatedT*> reallocated = static_cast<gsl::owner<AllocatedT*>>(std::realloc(_data.ptr(), new_capacity));
                assert(reallocated != nullptr);
                _data = tagged_data_ptr_t{reallocated, bool_ptr_tag{true}};
                goto done;
            }
        }
        {
            gsl::owner<AllocatedT*> allocated = alloc_handled(new_capacity);
            std::memcpy(allocated, _data.ptr(), _capacity);
            _data = tagged_data_ptr_t{allocated, bool_ptr_tag{true}};
        }
        done:
        _capacity = new_capacity;
    }

public:
    constexpr void clear () {
        _position = 0;
    }

    template<typename T>
    constexpr void go_back_to (Index<T> idx) {
        assert(idx.value <= _position);
        _position = idx.value;
    }

    template <typename T>
    [[nodiscard]] constexpr View<T> next (U count) {
        if constexpr (alignof(T) == 1) {
            const U start = allocate(count * sizeof(T));
            return View<T>{start, count};
        } else {
            const auto start = math::next_multiple<U, alignof(T)>(_position);
            const auto padding = start - _position;

            allocate(padding + (count * sizeof(T)));
            return View<T>{start, count};
        }
    }

    template <typename T>
    [[nodiscard]] constexpr Index<T> next () {
        if constexpr (sizeof(T) == 1) {
            if (_position == _capacity) {
                grow();
            }
            return Index<T>{_position++};
        } else {
            if constexpr (alignof(T) == 1) {
                const U start = allocate(sizeof(T));
                return Index<T>{start};
            } else {
                const auto start = math::next_multiple<U, alignof(T)>(_position);
                const auto padding = start - _position;

                allocate(padding + sizeof(T));
                return Index<T>{start};
            }
        }
    }

    template <typename T>
    constexpr T& get_next () {
        return Base::get(next<T>());
    }

    template <typename T>
    constexpr std::span<T> get_next (U count) {
        return Base::get(next<T>(count));
    }
};

template <std::unsigned_integral U>
struct ReadOnlyMemoryView : MemoryBase<ReadOnlyMemoryView<U>, U> {
    using Base = MemoryBase<ReadOnlyMemoryView<U>, U>;

    template <typename T>
    using Index = Base::template Index<T>;

    template <typename T>
    using Span = Base::template Span<T>;

    template <typename T>
    using View = Base::template View<T>;

private:
    uint8_t* _data;

public:
    constexpr explicit ReadOnlyMemoryView (uint8_t* data) : _data(data) {}
    template <typename AllocatedT>
    constexpr explicit ReadOnlyMemoryView (const Memory<U, AllocatedT>& memory) : _data(memory.data()) {}

    [[nodiscard]] constexpr uint8_t* const& data () const {
        return _data;
    }
};

using Buffer = Memory<uint32_t, max_align_t>;
using ReadOnlyBufferView = ReadOnlyMemoryView<uint32_t>;

template <typename ARRAY_TYPE, typename ELEMENT_TYPE, size_t LENGTH>
constexpr size_t MEMORY_INIT_ARRAY_SIZE = ((LENGTH * sizeof(ELEMENT_TYPE)) + sizeof(ARRAY_TYPE) - 1) / sizeof(ARRAY_TYPE);

template <typename ELEMENT_TYPE, size_t LENGTH>
constexpr size_t BUFFER_INIT_ARRAY_SIZE = MEMORY_INIT_ARRAY_SIZE<Buffer::backing_t, ELEMENT_TYPE, LENGTH>;
