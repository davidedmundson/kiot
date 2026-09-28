
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace lva::audio {

class PcmRingBuffer;
class WebRtcProcessor;

class AudioCapture {
   public:
    enum class Backend {
        kPulse,   // libpulse-simple from default source (legacy)
        kAlsa,    // alsa-lib direct (required for hardware-loopback AEC)
    };

    struct Options {
        Backend backend = Backend::kPulse;

        // ----- pulse backend -----
        std::string source;

        // ----- alsa backend -----
        // hw:0,4 exposes ch1/ch2 = mic, ch3/ch4 = codec loopback ref.
        std::string alsa_device   = "hw:0,4";
        unsigned    alsa_channels = 4;
        unsigned    mic_channel   = 0;   // 0-based mic channel

        // 0-based AEC reference channels. Two >=0 → downmixed to mono;
        // second = -1 → single-channel ref; first = -1 → AEC disabled.
        std::array<int, 2> ref_channels = {2, 3};

        // Frames per read; for alsa this is also the period size and
        // must equal the WebRTC APM frame (160 = 10 ms).
        std::size_t frames_per_read = 160;

        // Pulse-only: requested capture latency in microseconds.
        std::uint32_t buffer_latency_us = 100'000;
    };

    AudioCapture(const Options& opts, PcmRingBuffer& ring);
    ~AudioCapture();

    AudioCapture(const AudioCapture&)            = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    void AddTap(PcmRingBuffer& ring);

    void SetProcessor(WebRtcProcessor* processor);

    void SetMicVolumeSource(const std::atomic<int>* vol) {
        mic_volume_ptr_ = vol;
    }

    bool Start();

    void Stop();

    bool IsRunning() const noexcept {
        return running_.load(std::memory_order_relaxed);
    }

    std::uint64_t SamplesCaptured() const noexcept {
        return samples_captured_.load(std::memory_order_relaxed);
    }

   private:
    void ThreadLoopPulse();
    void ThreadLoopAlsa();

    void PostFrame(std::int16_t* mic, std::size_t n);

    Options                  opts_;
    PcmRingBuffer&           ring_;
    std::vector<PcmRingBuffer*> taps_;
    WebRtcProcessor*         processor_ = nullptr;

    const std::atomic<int>*  mic_volume_ptr_ = nullptr;

    void*                    pa_handle_  = nullptr;
    void*                    alsa_handle_ = nullptr;

    std::thread              thread_;
    std::atomic<bool>        running_{false};
    std::atomic<bool>        stop_requested_{false};
    std::atomic<std::uint64_t> samples_captured_{0};

    std::uint64_t            dropped_samples_  = 0;
    std::uint64_t            last_drop_log_at_ = 0;
};

}  // namespace lva::audio
