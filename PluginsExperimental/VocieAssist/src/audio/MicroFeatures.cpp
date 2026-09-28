#include "audio/MicroFeatures.h"

#include <cstdlib>
#include <cstring>

extern "C" {
#include "tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend_util.h"
}

#include "util/Log.h"

namespace lva::audio {

namespace {

constexpr const char* kTag = "features";

void InitConfig(FrontendConfig* cfg) {
    cfg->window.size_ms      = MicroFeatures::kFeatureWindowMs;  // 30
    cfg->window.step_size_ms = MicroFeatures::kFeatureStrideMs;  // 10

    cfg->filterbank.num_channels      = MicroFeatures::kFeatureSize;  // 40
    cfg->filterbank.lower_band_limit  = 125.0f;
    cfg->filterbank.upper_band_limit  = 7500.0f;

    cfg->noise_reduction.smoothing_bits        = 10;
    cfg->noise_reduction.even_smoothing        = 0.025f;
    cfg->noise_reduction.odd_smoothing         = 0.06f;
    cfg->noise_reduction.min_signal_remaining  = 0.05f;

    cfg->pcan_gain_control.enable_pcan = 1;
    cfg->pcan_gain_control.strength    = 0.95f;
    cfg->pcan_gain_control.offset      = 80.0f;
    cfg->pcan_gain_control.gain_bits   = 21;

    cfg->log_scale.enable_esphome_log  = 1;
    cfg->log_scale.scale_shift = 6;
}

}  // namespace

MicroFeatures::MicroFeatures() {
    config_ = static_cast<FrontendConfig*>(
        std::calloc(1, sizeof(FrontendConfig)));
    state_ = static_cast<FrontendState*>(
        std::calloc(1, sizeof(FrontendState)));
    if (config_ == nullptr || state_ == nullptr) {
        LVA_LOGE(kTag, "calloc failed");
        std::free(config_);
        std::free(state_);
        config_ = nullptr;
        state_  = nullptr;
        return;
    }

    InitConfig(config_);

    if (!FrontendPopulateState(config_, state_, kSampleRateHz)) {
        LVA_LOGE(kTag, "FrontendPopulateState failed");
        std::free(config_);
        std::free(state_);
        config_ = nullptr;
        state_  = nullptr;
        return;
    }

    leftover_.reserve(kSamplesPerChunk);
}

MicroFeatures::~MicroFeatures() {
    if (state_ != nullptr) {
        FrontendFreeStateContents(state_);
        std::free(state_);
    }
    std::free(config_);
}

void MicroFeatures::Reset() {
    if (state_ == nullptr) return;
    FrontendFreeStateContents(state_);
    std::memset(state_, 0, sizeof(FrontendState));
    if (!FrontendPopulateState(config_, state_, kSampleRateHz)) {
        LVA_LOGE(kTag, "FrontendPopulateState failed during Reset");
        std::free(state_);
        state_ = nullptr;
    }
    leftover_.clear();
}

void MicroFeatures::ProcessOneChunk(const std::int16_t* chunk,
                                    std::vector<float>& out) {
    std::size_t samples_read = 0;
    FrontendOutput fo = FrontendProcessSamples(
        state_,
        chunk,
        kSamplesPerChunk,
        &samples_read);
    if (fo.values == nullptr || fo.size == 0) {
        // Not yet primed — first ~30 ms of audio produces no output.
        return;
    }
    const std::size_t before = out.size();
    out.resize(before + fo.size);
    for (std::size_t i = 0; i < fo.size; ++i) {
        out[before + i] = static_cast<float>(fo.values[i]) * kFloat32Scale;
    }
}

std::size_t MicroFeatures::Process(const std::int16_t* samples,
                                   std::size_t n,
                                   std::vector<float>& out) {
    if (state_ == nullptr) return 0;

    const std::size_t before = out.size();

    std::size_t consumed = 0;

    // 1. If we have leftover samples, fill the chunk and consume.
    if (!leftover_.empty()) {
        const std::size_t need = kSamplesPerChunk - leftover_.size();
        const std::size_t take = std::min(need, n);
        leftover_.insert(leftover_.end(), samples, samples + take);
        consumed += take;
        if (leftover_.size() == kSamplesPerChunk) {
            ProcessOneChunk(leftover_.data(), out);
            leftover_.clear();
        }
    }

    // 2. Process full chunks straight from the input.
    while (consumed + kSamplesPerChunk <= n) {
        ProcessOneChunk(samples + consumed, out);
        consumed += kSamplesPerChunk;
    }

    // 3. Stash the tail into leftover_ for the next call.
    if (consumed < n) {
        leftover_.insert(leftover_.end(),
                         samples + consumed,
                         samples + n);
    }

    return (out.size() - before) / kFeatureSize;
}

}  // namespace lva::audio
