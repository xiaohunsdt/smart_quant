#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace sq {

// Fixed-capacity, zero-allocation slab memory pool.
//
// All N objects are pre-constructed at startup.  alloc() and free() are O(1)
// and never call malloc/new at runtime.  Not thread-safe — use one pool per
// thread or add external locking.
template <typename T, std::size_t N>
class MemoryPool {
public:
    static_assert(N > 0, "MemoryPool size must be > 0");

    MemoryPool() {
        // Build free list: each slot points to the next
        for (std::size_t i = 0; i < N - 1; ++i)
            slots_[i].next = &slots_[i + 1];
        slots_[N - 1].next = nullptr;
        free_head_ = &slots_[0];
        free_count_ = N;
    }

    // Returns a pointer to a zeroed-out T, or nullptr if the pool is exhausted.
    [[nodiscard]] T* alloc() noexcept {
        if (!free_head_) return nullptr;
        Slot* s    = free_head_;
        free_head_ = s->next;
        --free_count_;
        // Placement-construct (trivially zeroes POD types)
        return new (&s->storage) T{};
    }

    // Returns a pointer to an uninitialized T (caller must construct).
    [[nodiscard]] T* alloc_raw() noexcept {
        if (!free_head_) return nullptr;
        Slot* s    = free_head_;
        free_head_ = s->next;
        --free_count_;
        return reinterpret_cast<T*>(&s->storage);
    }

    // Release a previously alloc'd pointer back to the pool.
    void free(T* p) noexcept {
        if (!p) return;
        p->~T();
        auto* s    = reinterpret_cast<Slot*>(p);
        s->next    = free_head_;
        free_head_ = s;
        ++free_count_;
    }

    [[nodiscard]] std::size_t free_count()  const noexcept { return free_count_; }
    [[nodiscard]] std::size_t total_count() const noexcept { return N; }
    [[nodiscard]] bool        empty()       const noexcept { return free_count_ == 0; }

private:
    union Slot {
        alignas(T) std::byte storage[sizeof(T)];
        Slot* next;
    };

    std::array<Slot, N> slots_;
    Slot*       free_head_{nullptr};
    std::size_t free_count_{0};
};

}  // namespace sq
