
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "audio/MicroFeatures.h"
#include "audio/TfliteRuntime.h"

namespace lva::audio {

class MicroWakeWord {
   public:
    struct Config {
        // Wake word identifier (e.g. "okay_nabu") + display name.
        std::string id;
        std::string wake_word;        // e.g. "Okay Nabu"
        std::vector<std::string> trained_languages;

        // Path to the .tflite (resolved by Load()).
        std::filesystem::path model_path;

        // From the per-model JSON. Initial value only; the live
        // threshold is cutoff_live_ (atomic).
        float    probability_cutoff   = 0.85f;
        int      sliding_window_size  = 5;
    };

    MicroWakeWord();
    ~MicroWakeWord();

    MicroWakeWord(const MicroWakeWord&)            = delete;
    MicroWakeWord& operator=(const MicroWakeWord&) = delete;

    static std::unique_ptr<MicroWakeWord> FromConfig(
        const std::filesystem::path& json_path);

    // Returns true if the model and its TFLite runtime are ready.
    bool Ok() const noexcept { return ok_; }

    const Config& config() const noexcept { return cfg_; }

    // Live probability threshold. Written from the main thread,
    // read from the wake-word thread in Process(); atomic to avoid
    // a data race.
    void SetProbabilityCutoff(float v) noexcept {
        cutoff_live_.store(v, std::memory_order_relaxed);
    }
    float ProbabilityCutoff() const noexcept {
        return cutoff_live_.load(std::memory_order_relaxed);
    }

    bool Process(const float* frames,
                 std::size_t frame_count,
                 float* out_last_prob = nullptr);

    void Reset();

    float LastProbability() const noexcept { return last_prob_; }

   private:
    float InvokeOnce(const float* stride_features);

    Config cfg_;
    bool   ok_ = false;

    // Live, thread-safe copy of cfg_.probability_cutoff.
    std::atomic<float> cutoff_live_{0.85f};

    TfliteRuntime runtime_;

    int      stride_                = 3;     // input shape [1, stride, 40]
    float    input_scale_           = 0.0f;
    int      input_zero_point_      = 0;
    float    output_scale_          = 0.0f;
    int      output_zero_point_     = 0;
    std::size_t input_byte_size_    = 0;
    std::size_t output_byte_size_   = 0;

    std::vector<float>   features_buffer_;
    std::vector<std::uint8_t> quant_input_;
    std::vector<std::uint8_t> quant_output_;

    std::deque<float>    probabilities_;

    float last_prob_ = 0.0f;
};

}  // namespace lva::audio
