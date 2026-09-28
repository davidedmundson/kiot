// OpenWakeWordFeatures: shared mel-spectrogram + embedding extraction.
// One instance per process, feeds all OpenWakeWord keyword models.

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "audio/TfliteRuntime.h"

namespace lva::audio {

class OpenWakeWordFeatures {
   public:
    static constexpr int kSampleRate       = 16000;
    static constexpr int kMelSamples       = 1760;
    static constexpr int kSamplesPerChunk  = 1280;
    static constexpr int kNumMels          = 32;
    static constexpr int kEmbFeatures      = 76;
    static constexpr int kEmbStep          = 8;
    static constexpr int kWwFeatures       = 96;
    static constexpr int kMaxSeconds       = 10;
    static constexpr int kMaxSamples       = kMaxSeconds * kSampleRate;
    static constexpr int kMelsPerSecond    = 97;
    static constexpr int kMaxMels          = kMaxSeconds * kMelsPerSecond; // 970
    static constexpr int kMaxEmb           = kMaxSeconds * kEmbStep;       // 80
    static constexpr int kAutofillSamples  = 8 * kSampleRate;

    OpenWakeWordFeatures();
    bool Load(const std::filesystem::path& models_dir);
    bool Ok() const noexcept { return ok_; }

    // Feed raw int16 PCM. Returns embeddings (each kWwFeatures floats).
    // Output may contain 0 or more embeddings per call.
    void Process(const std::int16_t* samples, std::size_t count,
                 std::vector<float>& out_embeddings);

    void Reset();

   private:
    bool ok_ = false;
    TfliteRuntime mel_runtime_;
    TfliteRuntime emb_runtime_;

    std::vector<float> audio_buf_;
    int new_audio_samples_ = kAutofillSamples;

    std::vector<float> mels_buf_;
    int new_mels_ = 0;
};

}  // namespace lva::audio
