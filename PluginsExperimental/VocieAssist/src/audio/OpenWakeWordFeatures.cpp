#include "audio/OpenWakeWordFeatures.h"

#include <algorithm>
#include <cstring>

#include "util/Log.h"

namespace lva::audio {

namespace {
constexpr const char* kTag = "oww_feat";
}

OpenWakeWordFeatures::OpenWakeWordFeatures() {
    audio_buf_.resize(kMaxSamples, 0.0f);
    mels_buf_.resize(kMaxMels * kNumMels, 0.0f);
}

bool OpenWakeWordFeatures::Load(const std::filesystem::path& models_dir) {
    const auto mel_path = models_dir / "melspectrogram.tflite";
    const auto emb_path = models_dir / "embedding_model.tflite";

    if (!mel_runtime_.Ok()) return false;
    if (!mel_runtime_.LoadModel(mel_path.string())) {
        LVA_LOGE(kTag, "failed to load %s", mel_path.c_str());
        return false;
    }
    // Resize mel input to [1, kMelSamples]
    if (!mel_runtime_.ResizeInput(0, {1, kMelSamples})) {
        LVA_LOGE(kTag, "failed to resize mel input");
        return false;
    }

    if (!emb_runtime_.Ok()) return false;
    if (!emb_runtime_.LoadModel(emb_path.string())) {
        LVA_LOGE(kTag, "failed to load %s", emb_path.c_str());
        return false;
    }
    // Resize emb input to [1, kEmbFeatures, kNumMels, 1]
    if (!emb_runtime_.ResizeInput(0, {1, kEmbFeatures, kNumMels, 1})) {
        LVA_LOGE(kTag, "failed to resize emb input");
        return false;
    }

    ok_ = true;
    LVA_LOGI(kTag, "loaded mel+embedding models from %s",
             models_dir.c_str());
    return true;
}

void OpenWakeWordFeatures::Process(const std::int16_t* samples,
                                   std::size_t count,
                                   std::vector<float>& out_embeddings) {
    if (!ok_) return;

    // Convert int16 to float and shift into audio_buf_
    const std::size_t n = std::min(count, static_cast<std::size_t>(kMaxSamples));
    if (n >= static_cast<std::size_t>(kMaxSamples)) {
        for (std::size_t i = 0; i < n; ++i)
            audio_buf_[i] = static_cast<float>(samples[i]);
        new_audio_samples_ = kMaxSamples;
    } else {
        // Shift left
        std::memmove(audio_buf_.data(),
                     audio_buf_.data() + n,
                     (kMaxSamples - n) * sizeof(float));
        for (std::size_t i = 0; i < n; ++i)
            audio_buf_[kMaxSamples - n + i] = static_cast<float>(samples[i]);
        new_audio_samples_ = std::min(kMaxSamples, new_audio_samples_ + static_cast<int>(n));
    }

    // Generate mel spectrograms
    while (new_audio_samples_ >= kMelSamples) {
        const int start = kMaxSamples - new_audio_samples_;
        // Copy audio window to mel input
        mel_runtime_.CopyInput(0, audio_buf_.data() + start,
                                 kMelSamples * sizeof(float));
        new_audio_samples_ = std::max(0, new_audio_samples_ - kSamplesPerChunk);

        if (!mel_runtime_.Invoke()) {
            LVA_LOGW(kTag, "mel invoke failed");
            continue;
        }
        const auto mel_out_size = mel_runtime_.OutputInfo(0).byte_size;
        if (mel_out_size == 0) continue;
        const int mel_floats = static_cast<int>(mel_out_size / sizeof(float));
        if (mel_floats < kNumMels) continue;
        static bool mel_esphome_logged = false;
        if (!mel_esphome_logged) {
            LVA_LOGI(kTag, "mel output: %zu bytes (%d floats, %d windows)",
                     mel_out_size, mel_floats, mel_floats / kNumMels);
            mel_esphome_logged = true;
        }
        std::vector<float> mel_out(mel_floats);
        mel_runtime_.CopyOutput(0, mel_out.data(), mel_out_size);

        for (auto& v : mel_out) v = v / 10.0f + 2.0f;

        const int num_windows = mel_floats / kNumMels;
        if (num_windows > kMaxMels) {
            LVA_LOGW(kTag, "mel output too large: %d windows", num_windows);
            continue;
        }

        const int total_mels = kMaxMels * kNumMels;
        std::memmove(mels_buf_.data(),
                     mels_buf_.data() + num_windows * kNumMels,
                     (total_mels - num_windows * kNumMels) * sizeof(float));
        // Overwrite end
        std::memcpy(mels_buf_.data() + total_mels - num_windows * kNumMels,
                    mel_out.data(),
                    num_windows * kNumMels * sizeof(float));
        new_mels_ = std::min(kMaxMels, new_mels_ + num_windows);

        // Generate embeddings
        while (new_mels_ >= kEmbFeatures) {
            const int mel_start = (kMaxMels - new_mels_) * kNumMels;
            // EMB input shape: [1, kEmbFeatures, kNumMels, 1]
            std::vector<float> emb_input(kEmbFeatures * kNumMels, 0.0f);
            std::memcpy(emb_input.data(),
                        mels_buf_.data() + mel_start,
                        kEmbFeatures * kNumMels * sizeof(float));
            new_mels_ = std::max(0, new_mels_ - kEmbStep);

            emb_runtime_.CopyInput(0, emb_input.data(),
                                     emb_input.size() * sizeof(float));
            if (!emb_runtime_.Invoke()) {
                LVA_LOGW(kTag, "emb invoke failed");
                break;
            }

            const auto emb_out_size = emb_runtime_.OutputInfo(0).byte_size;
            if (emb_out_size == 0) break;
            const int emb_floats = static_cast<int>(emb_out_size / sizeof(float));
            const std::size_t prev = out_embeddings.size();
            out_embeddings.resize(prev + emb_floats);
            emb_runtime_.CopyOutput(0, out_embeddings.data() + prev,
                                        emb_out_size);
        }
    }
}

void OpenWakeWordFeatures::Reset() {
    std::fill(audio_buf_.begin(), audio_buf_.end(), 0.0f);
    std::fill(mels_buf_.begin(), mels_buf_.end(), 0.0f);
    new_audio_samples_ = kAutofillSamples;
    new_mels_ = 0;
}

}  // namespace lva::audio
