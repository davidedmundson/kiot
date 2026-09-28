
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

namespace webrtc { class AudioProcessing; class StreamConfig; }

namespace lva::audio {

class WebRtcProcessor {
   public:
    static constexpr int kSampleRate     = 16'000;
    static constexpr int kFrameSamples   = 160;   // 10 ms @ 16 kHz

    WebRtcProcessor(int agc_level, int ns_level, bool aec_enabled);
    ~WebRtcProcessor();

    WebRtcProcessor(const WebRtcProcessor&)            = delete;
    WebRtcProcessor& operator=(const WebRtcProcessor&) = delete;

    void SetLevels(int agc_level, int ns_level);

    // Enable/disable actual echo cancellation at runtime. AEC must only
    // run while there is genuine far-end playback (music/TTS), otherwise
    // the hardware-loopback reference carries only crosstalk (~25 dB
    // hotter than the mic, weakly correlated) and AEC3's residual/NLP
    // stage mistakes near-end speech for echo and zero-gates it. Called
    // from the main thread on player state changes; no-op when AEC was
    // not configured (no reference channels). Cheap: the capture thread
    // applies the toggle only on transition.
    void SetFarEndActive(bool active);

    bool Process(std::int16_t* buf, std::size_t n);

    bool ProcessReverse(std::int16_t* buf, std::size_t n);

    // Reset the echo canceller (rebuild the APM) so it re-converges
    // from a clean state. Call after an ALSA xrun/recover, where the
    // mic and reference streams are no longer aligned. No-op when AEC
    // is disabled.
    void ResetEcho();

    int  agc_level()   const { return agc_level_; }
    int  ns_level()    const { return ns_level_; }
    bool aec_enabled() const { return aec_enabled_; }

   private:
    // Caller must hold apm_mutex_.
    void RebuildLocked();
    // Build the APM Config from the current agc/ns/aec fields, with the
    // echo canceller enabled only when echo_on is true. Caller must hold
    // apm_mutex_.
    void ApplyConfigLocked(bool echo_on);
    // Reconcile the applied echo state with the desired far-end gate.
    // Caller must hold apm_mutex_.
    void ReconcileGateLocked();

    // Serializes all access to apm_: Process()/ProcessReverse()/
    // ResetEcho() run on the capture thread, SetLevels() on the main
    // thread, so APM rebuilds must not race.
    std::mutex apm_mutex_;

    int  agc_level_   = 0;
    int  ns_level_    = 0;
    bool aec_enabled_ = false;

    // Desired far-end state (main thread) vs. the echo-canceller state
    // currently applied to the APM (capture thread). The capture thread
    // reconciles them on transition inside Process().
    std::atomic<bool> far_end_active_{false};
    bool              echo_on_applied_ = false;

    void* apm_ = nullptr;
    std::unique_ptr<webrtc::StreamConfig> stream_cfg_;
};

}  // namespace lva::audio
