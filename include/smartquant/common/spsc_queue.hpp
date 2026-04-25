#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace sq {

// Lock-free, single-producer / single-consumer ring queue.
//
// Capacity must be a power of two.  push() and pop() are wait-free O(1).
// The producer and consumer may reside on different threads; no other
// concurrent access is safe.
template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "SpscQueue Capacity must be a power of two");

public:
    // Returns true if the item was enqueued, false if the queue is full.
    bool push(const T& item) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire))
            return false;  // full
        buf_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool push(T&& item) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire))
            return false;
        buf_[head] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Returns the item if available, or std::nullopt if empty.
    std::optional<T> pop() noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return std::nullopt;  // empty
        T item = buf_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return item;
    }

    // Non-allocating pop: fills out and returns true, or returns false.
    bool pop(T& out) noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return false;
        out = buf_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return (head_.load(std::memory_order_acquire) -
                tail_.load(std::memory_order_acquire)) & kMask;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity - 1; }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::array<T, Capacity> buf_{};
};

}  // namespace sq
