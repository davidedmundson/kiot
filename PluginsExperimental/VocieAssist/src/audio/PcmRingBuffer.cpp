#include "audio/PcmRingBuffer.h"

#include <algorithm>
#include <cstring>

namespace lva::audio {

namespace {

std::size_t RoundUpPow2(std::size_t v) noexcept {
    if (v <= 1) return 1;
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    if constexpr (sizeof(std::size_t) > 4) {
        v |= v >> 32;
    }
    return v + 1;
}

}  // namespace

PcmRingBuffer::PcmRingBuffer(std::size_t capacity_samples)
    : buf_(RoundUpPow2(capacity_samples)),
      mask_(buf_.size() - 1) {}

std::size_t PcmRingBuffer::Size() const noexcept {
    const std::size_t h = head_.load(std::memory_order_acquire);
    const std::size_t t = tail_.load(std::memory_order_acquire);
    return h - t;  // unsigned wrap-around math gives the correct delta
}

std::size_t PcmRingBuffer::FreeSpace() const noexcept {
    return Capacity() - Size();
}

std::size_t PcmRingBuffer::Write(const std::int16_t* src, std::size_t n) {
    const std::size_t free = FreeSpace();
    const std::size_t to_write = std::min(n, free);
    if (to_write == 0) return 0;

    const std::size_t h        = head_.load(std::memory_order_relaxed);
    const std::size_t cap      = mask_ + 1;
    const std::size_t pos      = h & mask_;
    const std::size_t first    = std::min(to_write, cap - pos);

    std::memcpy(buf_.data() + pos, src, first * sizeof(std::int16_t));
    if (to_write > first) {
        std::memcpy(buf_.data(), src + first,
                    (to_write - first) * sizeof(std::int16_t));
    }

    head_.store(h + to_write, std::memory_order_release);
    return to_write;
}

std::size_t PcmRingBuffer::Read(std::int16_t* dst, std::size_t n) {
    const std::size_t avail   = Size();
    const std::size_t to_read = std::min(n, avail);
    if (to_read == 0) return 0;

    const std::size_t t      = tail_.load(std::memory_order_relaxed);
    const std::size_t cap    = mask_ + 1;
    const std::size_t pos    = t & mask_;
    const std::size_t first  = std::min(to_read, cap - pos);

    std::memcpy(dst, buf_.data() + pos, first * sizeof(std::int16_t));
    if (to_read > first) {
        std::memcpy(dst + first, buf_.data(),
                    (to_read - first) * sizeof(std::int16_t));
    }

    tail_.store(t + to_read, std::memory_order_release);
    return to_read;
}

void PcmRingBuffer::Reset() noexcept {
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
}

}  // namespace lva::audio
