
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lva::audio {

class PcmRingBuffer {
   public:
    explicit PcmRingBuffer(std::size_t capacity_samples);

    PcmRingBuffer(const PcmRingBuffer&)            = delete;
    PcmRingBuffer& operator=(const PcmRingBuffer&) = delete;

    std::size_t Capacity() const noexcept { return mask_ + 1; }

    std::size_t Size() const noexcept;

    std::size_t FreeSpace() const noexcept;

    std::size_t Write(const std::int16_t* src, std::size_t n);

    std::size_t Read(std::int16_t* dst, std::size_t n);

    void Reset() noexcept;

   private:
    std::vector<std::int16_t> buf_;
    std::size_t               mask_;
    std::atomic<std::size_t>  head_{0};
    std::atomic<std::size_t>  tail_{0};
};

}  // namespace lva::audio
