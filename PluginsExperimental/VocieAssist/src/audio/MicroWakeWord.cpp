#include "audio/MicroWakeWord.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numeric>
#include <sstream>

#include <nlohmann/json.hpp>

#include "util/Log.h"

namespace lva::audio {

namespace {

constexpr const char* kTag = "wakeword";
constexpr int kMaxSlidingWindow = 256;

}  // namespace

MicroWakeWord::MicroWakeWord() = default;
MicroWakeWord::~MicroWakeWord() = default;

std::unique_ptr<MicroWakeWord> MicroWakeWord::FromConfig(
    const std::filesystem::path& json_path) {
    std::ifstream f(json_path);
    if (!f) {
        LVA_LOGE(kTag, "cannot open config: %s", json_path.c_str());
        return nullptr;
    }
    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        LVA_LOGE(kTag, "JSON parse failed for %s: %s",
                 json_path.c_str(), e.what());
        return nullptr;
    }

    auto out = std::make_unique<MicroWakeWord>();
    Config& cfg = out->cfg_;

    cfg.id        = json_path.stem().string();
    cfg.wake_word = j.value("wake_word", cfg.id);
    if (auto it = j.find("trained_languages"); it != j.end() && it->is_array()) {
        for (const auto& l : *it) {
            if (l.is_string()) cfg.trained_languages.push_back(l.get<std::string>());
        }
    }

    const std::string model_rel = j.value("model", cfg.id + ".tflite");
    cfg.model_path = json_path.parent_path() / model_rel;

    if (auto it = j.find("micro"); it != j.end() && it->is_object()) {
        if (auto p = it->find("probability_cutoff"); p != it->end() && p->is_number()) {
            cfg.probability_cutoff = p->get<float>();
        }
        if (auto p = it->find("sliding_window_size"); p != it->end() && p->is_number_integer()) {
            cfg.sliding_window_size = p->get<int>();
        }
    }
    cfg.sliding_window_size = std::clamp(cfg.sliding_window_size, 1, kMaxSlidingWindow);

    // Load .tflite + cache tensor metadata.
    if (!out->runtime_.Ok()) {
        LVA_LOGE(kTag, "TFLite runtime not available: %s",
                 out->runtime_.LastError().c_str());
        return nullptr;
    }
    if (!out->runtime_.LoadModel(cfg.model_path.string())) {
        LVA_LOGE(kTag, "load %s failed: %s",
                 cfg.model_path.c_str(),
                 out->runtime_.LastError().c_str());
        return nullptr;
    }

    const auto in_info  = out->runtime_.InputInfo(0);
    const auto out_info = out->runtime_.OutputInfo(0);

    if (in_info.shape.size() != 3 || in_info.shape[2] != static_cast<int>(MicroFeatures::kFeatureSize)) {
        LVA_LOGE(kTag, "%s: unexpected input shape (need [1, stride, %u])",
                 cfg.id.c_str(),
                 static_cast<unsigned>(MicroFeatures::kFeatureSize));
        return nullptr;
    }
    out->stride_            = in_info.shape[1];
    out->input_scale_       = in_info.quant.scale;
    out->input_zero_point_  = in_info.quant.zero_point;
    out->output_scale_      = out_info.quant.scale;
    out->output_zero_point_ = out_info.quant.zero_point;
    out->input_byte_size_   = in_info.byte_size;
    out->output_byte_size_  = out_info.byte_size;

    if (out->input_scale_ == 0.0f) {
        LVA_LOGE(kTag, "%s: input tensor not quantized (scale=0); "
                       "C++ runner only supports quantized models",
                 cfg.id.c_str());
        return nullptr;
    }
    if (out->output_scale_ == 0.0f) {
        LVA_LOGE(kTag, "%s: output tensor not quantized", cfg.id.c_str());
        return nullptr;
    }

    if (!out->runtime_.ResizeInput(
            0, in_info.shape)) {
        LVA_LOGE(kTag, "%s: failed to resize input: %s",
                 cfg.id.c_str(), out->runtime_.LastError().c_str());
        return nullptr;
    }

    out->features_buffer_.reserve(out->stride_ * MicroFeatures::kFeatureSize);
    out->quant_input_.resize(out->input_byte_size_);
    out->quant_output_.resize(out->output_byte_size_);

    LVA_LOGI(kTag,
             "loaded '%s' from %s (stride=%d cutoff=%.3f window=%d "
             "in_scale=%.6f in_zp=%d out_scale=%.6f out_zp=%d)",
             cfg.id.c_str(), cfg.model_path.c_str(),
             out->stride_, cfg.probability_cutoff, cfg.sliding_window_size,
             out->input_scale_, out->input_zero_point_,
             out->output_scale_, out->output_zero_point_);

    // Seed the live (atomic) cutoff from the parsed config value.
    out->cutoff_live_.store(cfg.probability_cutoff,
                            std::memory_order_relaxed);
    out->ok_ = true;
    return out;
}

void MicroWakeWord::Reset() {
    features_buffer_.clear();
    probabilities_.clear();
    last_prob_ = 0.0f;
}

float MicroWakeWord::InvokeOnce(const float* stride_features) {
    const std::size_t total =
        static_cast<std::size_t>(stride_) * MicroFeatures::kFeatureSize;
    if (quant_input_.size() != input_byte_size_) {
        quant_input_.resize(input_byte_size_);
    }
    const bool input_is_int8 =
        runtime_.InputInfo(0).type == TfliteType::kInt8;
    for (std::size_t i = 0; i < total; ++i) {
        const float q = std::round(stride_features[i] / input_scale_
                                   + static_cast<float>(input_zero_point_));
        const int qi = static_cast<int>(q);
        int clamped;
        if (input_is_int8) {
            clamped = std::clamp(qi, -128, 127);
            quant_input_[i] = static_cast<std::uint8_t>(clamped & 0xff);
        } else {
            clamped = std::clamp(qi, 0, 255);
            quant_input_[i] = static_cast<std::uint8_t>(clamped);
        }
    }

    if (!runtime_.CopyInput(0, quant_input_.data(), input_byte_size_)) {
        LVA_LOGW(kTag, "[%s] CopyInput failed: %s",
                 cfg_.id.c_str(), runtime_.LastError().c_str());
        return last_prob_;
    }
    if (!runtime_.Invoke()) {
        LVA_LOGW(kTag, "[%s] Invoke failed: %s",
                 cfg_.id.c_str(), runtime_.LastError().c_str());
        return last_prob_;
    }
    if (!runtime_.CopyOutput(0, quant_output_.data(), output_byte_size_)) {
        LVA_LOGW(kTag, "[%s] CopyOutput failed: %s",
                 cfg_.id.c_str(), runtime_.LastError().c_str());
        return last_prob_;
    }

    const bool output_is_int8 =
        runtime_.OutputInfo(0).type == TfliteType::kInt8;
    int q;
    if (output_is_int8) {
        q = static_cast<int>(static_cast<int8_t>(quant_output_[0]));
    } else {
        q = static_cast<int>(quant_output_[0]);
    }
    const float prob = (static_cast<float>(q)
                        - static_cast<float>(output_zero_point_))
                       * output_scale_;
    last_prob_ = prob;
    return prob;
}

bool MicroWakeWord::Process(const float* frames,
                            std::size_t frame_count,
                            float* out_last_prob) {
    bool detected = false;
    if (!ok_) return false;

    for (std::size_t f = 0; f < frame_count; ++f) {
        // Append one frame to the accumulator.
        features_buffer_.insert(
            features_buffer_.end(),
            frames + f * MicroFeatures::kFeatureSize,
            frames + (f + 1) * MicroFeatures::kFeatureSize);

        // Once we have `stride_` frames, run an invoke and clear.
        const std::size_t target =
            static_cast<std::size_t>(stride_) * MicroFeatures::kFeatureSize;
        if (features_buffer_.size() < target) continue;

        const float prob = InvokeOnce(features_buffer_.data());
        features_buffer_.clear();

        // Sliding-window smoothing.
        probabilities_.push_back(prob);
        if (static_cast<int>(probabilities_.size()) > cfg_.sliding_window_size) {
            probabilities_.pop_front();
        }

        if (static_cast<int>(probabilities_.size()) < cfg_.sliding_window_size) {
            continue;  // window not yet full
        }

        const float mean = std::accumulate(
                              probabilities_.begin(),
                              probabilities_.end(), 0.0f)
                          / static_cast<float>(probabilities_.size());
        const float cutoff = cutoff_live_.load(std::memory_order_relaxed);
        if (mean > cutoff) {
            LVA_LOGD(kTag, "[%s] detected (mean=%.3f cutoff=%.3f)",
                     cfg_.id.c_str(), mean, cutoff);
            detected = true;
        }
    }

    if (out_last_prob) *out_last_prob = last_prob_;
    return detected;
}

}  // namespace lva::audio
