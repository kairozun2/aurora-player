// Aurora Player - lock-free single-producer / single-consumer float ring buffer.
// The decode thread writes, the audio callback reads: no mutex on the hot path.
#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace aurora {

class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity = 1 << 16) { reset(capacity); }

    void reset(std::size_t capacity) {
        buffer_.assign(capacity + 1, 0.0f); // one slot kept free to tell full from empty
        read_.store(0, std::memory_order_relaxed);
        write_.store(0, std::memory_order_relaxed);
    }

    void clear() {
        read_.store(0, std::memory_order_release);
        write_.store(0, std::memory_order_release);
    }

    std::size_t capacity() const { return buffer_.size() - 1; }

    std::size_t available() const {
        const std::size_t w = write_.load(std::memory_order_acquire);
        const std::size_t r = read_.load(std::memory_order_acquire);
        return w >= r ? w - r : buffer_.size() - r + w;
    }

    std::size_t space() const { return capacity() - available(); }

    double fill() const {
        const std::size_t cap = capacity();
        return cap == 0 ? 0.0 : static_cast<double>(available()) / static_cast<double>(cap);
    }

    /// Writes up to `count` samples; returns how many were accepted.
    std::size_t write(const float* data, std::size_t count) {
        const std::size_t writable = space();
        const std::size_t n = count < writable ? count : writable;
        std::size_t w = write_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < n; ++i) {
            buffer_[w] = data[i];
            w = (w + 1 == buffer_.size()) ? 0 : w + 1;
        }
        write_.store(w, std::memory_order_release);
        return n;
    }

    /// Reads up to `count` samples; returns how many were produced.
    std::size_t read(float* out, std::size_t count) {
        const std::size_t readable = available();
        const std::size_t n = count < readable ? count : readable;
        std::size_t r = read_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < n; ++i) {
            out[i] = buffer_[r];
            r = (r + 1 == buffer_.size()) ? 0 : r + 1;
        }
        read_.store(r, std::memory_order_release);
        return n;
    }

private:
    std::vector<float> buffer_;
    std::atomic<std::size_t> read_{0};
    std::atomic<std::size_t> write_{0};
};

} // namespace aurora
