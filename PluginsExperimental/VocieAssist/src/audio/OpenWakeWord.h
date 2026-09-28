// OpenWakeWord: per-keyword model that consumes embeddings from
// OpenWakeWordFeatures and produces detection probabilities.

#pragma once

#include <atomic>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "audio/TfliteRuntime.h"

namespace lva::audio {

class OpenWakeWord {
   public:
    static constexpr int kWwFeatures = 96;
    static constexpr int kMaxEmb = 80; // 10s * 8 emb/sec

    struct Config {
        std::string id;
        std::string wake_word;
        std::vector<std::string> trained_languages;
        std::filesystem::path model_path;
        float probability_cutoff = 0.5f;
        int sliding_window_size = 5;
    };

    OpenWakeWord() = default;

    static std::unique_ptr<OpenWakeWord> FromConfig(
        const std::filesystem::path& json_path);

    bool Ok() const noexcept { return ok_; }
    const Config& config() const noexcept { return cfg_; }

    // Live probability threshold. Written from the main thread,
    // read from the wake-word thread in Process(); atomic to avoid a
    // data race.
    void SetProbabilityCutoff(float v) noexcept {
        cutoff_live_.store(v, std::memory_order_relaxed);
    }
    float ProbabilityCutoff() const noexcept {
        return cutoff_live_.load(std::memory_order_relaxed);
    }

    // Feed embeddings (multiples of kWwFeatures floats).
    // Returns true if detection threshold crossed.
    bool Process(const float* embeddings, std::size_t num_embeddings,
                 float* out_last_prob = nullptr);

    void Reset();

   private:
    Config cfg_;
    bool ok_ = false;
    std::atomic<float> cutoff_live_{0.5f};
    TfliteRuntime runtime_;
    int input_windows_ = 16;

    std::vector<float> emb_buf_; // [kMaxEmb, kWwFeatures]
    int new_embeddings_ = 0;
    std::deque<float> probabilities_;
    float last_prob_ = 0.0f;
};

}  // namespace lva::audio
