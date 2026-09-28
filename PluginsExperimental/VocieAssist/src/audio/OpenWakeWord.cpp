#include "audio/OpenWakeWord.h"

#include <algorithm>
#include <cstring>
#include <fstream>

#include <nlohmann/json.hpp>
#include "util/Log.h"

namespace lva::audio {

namespace {
constexpr const char* kTag = "oww";
}

std::unique_ptr<OpenWakeWord> OpenWakeWord::FromConfig(
    const std::filesystem::path& json_path) {
    std::ifstream f(json_path);
    if (!f) return nullptr;
    nlohmann::json j;
    try { f >> j; } catch (...) { return nullptr; }

    if (j.value("type", "") != "open") return nullptr;

    auto out = std::make_unique<OpenWakeWord>();
    out->cfg_.id = json_path.stem().string();
    out->cfg_.wake_word = j.value("wake_word", out->cfg_.id);
    out->cfg_.model_path = json_path.parent_path() / j.value("model", out->cfg_.id + ".tflite");
    if (j.contains("trained_languages") && j["trained_languages"].is_array()) {
        for (const auto& lang : j["trained_languages"])
            if (lang.is_string())
                out->cfg_.trained_languages.push_back(lang.get<std::string>());
    }
    if (j.contains("open") && j["open"].is_object()) {
        const auto& m = j["open"];
        out->cfg_.probability_cutoff = m.value("probability_cutoff", 0.5f);
        out->cfg_.sliding_window_size = m.value("sliding_window_size", 5);
    }

    if (!out->runtime_.Ok()) {
        LVA_LOGE(kTag, "TFLite runtime not available");
        return nullptr;
    }
    if (!out->runtime_.LoadModel(out->cfg_.model_path.string())) {
        LVA_LOGE(kTag, "failed to load %s", out->cfg_.model_path.c_str());
        return nullptr;
    }

    const auto info = out->runtime_.InputInfo(0);
    if (info.shape.size() >= 2 && info.shape[1] > 0) {
        out->input_windows_ = info.shape[1];
    }
    if (out->input_windows_ <= 0) {
        LVA_LOGE(kTag, "invalid input_windows for %s", out->cfg_.id.c_str());
        return nullptr;
    }

    if (!out->runtime_.ResizeInput(
            0, {1, out->input_windows_, kWwFeatures})) {
        LVA_LOGE(kTag, "failed to resize keyword input for %s: %s",
                 out->cfg_.id.c_str(), out->runtime_.LastError().c_str());
        return nullptr;
    }

    out->emb_buf_.resize(kMaxEmb * kWwFeatures, 0.0f);
    out->cutoff_live_.store(out->cfg_.probability_cutoff,
                            std::memory_order_relaxed);
    out->ok_ = true;

    LVA_LOGI(kTag, "loaded '%s' from %s (input_windows=%d cutoff=%.3f)",
             out->cfg_.id.c_str(), out->cfg_.model_path.c_str(),
             out->input_windows_, out->cfg_.probability_cutoff);
    return out;
}

bool OpenWakeWord::Process(const float* embeddings, std::size_t num_embeddings,
                           float* out_last_prob) {
    if (!ok_) return false;
    if (input_windows_ <= 0) return false;  // guard against bad shape

    const int num_emb_vectors = static_cast<int>(num_embeddings / kWwFeatures);
    if (num_emb_vectors == 0) return false;
    if (num_emb_vectors >= kMaxEmb) {
        // Too many embeddings at once — just keep the latest kMaxEmb.
        std::memcpy(emb_buf_.data(),
                    embeddings + (num_emb_vectors - kMaxEmb) * kWwFeatures,
                    kMaxEmb * kWwFeatures * sizeof(float));
        new_embeddings_ = kMaxEmb;
    } else {
        const int shift = num_emb_vectors * kWwFeatures;
        std::memmove(emb_buf_.data(),
                     emb_buf_.data() + shift,
                     (kMaxEmb * kWwFeatures - shift) * sizeof(float));
        std::memcpy(emb_buf_.data() + (kMaxEmb - num_emb_vectors) * kWwFeatures,
                    embeddings,
                    num_emb_vectors * kWwFeatures * sizeof(float));
        new_embeddings_ = std::min(kMaxEmb, new_embeddings_ + num_emb_vectors);
    }

    bool detected = false;

    while (new_embeddings_ >= input_windows_) {
        const int start = (kMaxEmb - new_embeddings_) * kWwFeatures;
        // Input: [1, input_windows_, kWwFeatures]
        if (!runtime_.CopyInput(0, emb_buf_.data() + start,
                                input_windows_ * kWwFeatures * sizeof(float))) {
            new_embeddings_ = std::max(0, new_embeddings_ - 1);
            break;
        }
        new_embeddings_ = std::max(0, new_embeddings_ - 1);

        if (!runtime_.Invoke()) break;

        // Output: single float probability
        float prob = 0.0f;
        if (!runtime_.CopyOutput(0, &prob, sizeof(float))) break;
        last_prob_ = prob;

        probabilities_.push_back(prob);
        while (static_cast<int>(probabilities_.size()) > cfg_.sliding_window_size) {
            probabilities_.pop_front();
        }

        if (static_cast<int>(probabilities_.size()) >= cfg_.sliding_window_size) {
            float sum = 0.0f;
            for (float p : probabilities_) sum += p;
            const float mean = sum / static_cast<float>(probabilities_.size());
            if (mean >= cutoff_live_.load(std::memory_order_relaxed)) {
                detected = true;
                probabilities_.clear();
                new_embeddings_ = 0;
            }
        }
    }

    if (out_last_prob) *out_last_prob = last_prob_;
    return detected;
}

void OpenWakeWord::Reset() {
    std::fill(emb_buf_.begin(), emb_buf_.end(), 0.0f);
    new_embeddings_ = 0;
    probabilities_.clear();
    last_prob_ = 0.0f;
}

}  // namespace lva::audio
