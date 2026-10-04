#pragma once
#include <atomic>
#include <array>
#include <thread>

template <typename T, size_t N>
class RingQueue {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");
public:
    RingQueue() : head_(0), tail_(0), published_(0) {}

    bool push(const T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        while (true) {
            size_t head = head_.load(std::memory_order_acquire);
            if (tail - head >= N) return false;
            if (tail_.compare_exchange_weak(tail, tail + 1,
                    std::memory_order_relaxed)) {
                buffer_[tail & (N - 1)] = item;
                size_t expected = tail;
                while (!published_.compare_exchange_weak(expected, tail + 1,
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                    std::this_thread::yield();
                }
                return true;
            }
        }
    }
    bool pop(T& out) {
        size_t head = head_.load(std::memory_order_relaxed);
        while (true) {
            size_t pub = published_.load(std::memory_order_acquire);
            if (head >= pub) return false;
            if (head_.compare_exchange_weak(head, head + 1,
                    std::memory_order_relaxed)) {
                out = buffer_[head & (N - 1)];
                return true;
            }
        }
    }
private:
    std::array<T, N> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
    std::atomic<size_t> published_;
};
