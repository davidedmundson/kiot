#include "audio/WakeWordEngine.h"

#include <chrono>
#include <utility>
#include <vector>

#include "audio/MicroFeatures.h"
#include "audio/MicroWakeWord.h"
#include "audio/OpenWakeWord.h"
#include "audio/OpenWakeWordFeatures.h"
#include "audio/PcmRingBuffer.h"
#include "util/Log.h"

namespace lva::audio {

namespace {
constexpr const char* kTag = "ww_engine";
constexpr std::size_t kPopSamples = 1024;
constexpr std::chrono::milliseconds kIdleSleep{10};
}  // namespace

WakeWordEngine::WakeWordEngine(PcmRingBuffer& ring) : ring_(ring) {}
WakeWordEngine::~WakeWordEngine() { Stop(); }

void WakeWordEngine::AddModel(std::shared_ptr<MicroWakeWord> model) {
    if (running_.load(std::memory_order_relaxed)) return;
    if (!model || !model->Ok()) return;
    LVA_LOGI(kTag, "registered model: %s", model->config().id.c_str());
    models_.push_back(std::move(model));
}

void WakeWordEngine::AddOpenModel(std::shared_ptr<OpenWakeWord> model) {
    if (running_.load(std::memory_order_relaxed)) return;
    if (!model || !model->Ok()) return;
    LVA_LOGI(kTag, "registered open model: %s", model->config().id.c_str());
    open_models_.push_back(std::move(model));
}

void WakeWordEngine::SetOpenFeatures(std::unique_ptr<OpenWakeWordFeatures> f) {
    open_features_ = std::move(f);
}

void WakeWordEngine::SetActiveModels(
    std::vector<std::shared_ptr<MicroWakeWord>> new_models) {
    std::lock_guard<std::mutex> lk(models_mtx_);
    for (const auto& m : models_) {
        if (m && m->config().id == "stop") {
            new_models.push_back(m);
        }
    }
    models_ = std::move(new_models);
    models_changed_ = true;
}

void WakeWordEngine::SetActiveOpenModels(
    std::vector<std::shared_ptr<OpenWakeWord>> new_models) {
    std::lock_guard<std::mutex> lk(models_mtx_);
    open_models_ = std::move(new_models);
    models_changed_ = true;
}

std::vector<std::shared_ptr<MicroWakeWord>>
WakeWordEngine::SnapshotModels() const {
    std::lock_guard<std::mutex> lk(models_mtx_);
    return models_;
}

std::vector<std::shared_ptr<OpenWakeWord>>
WakeWordEngine::SnapshotOpenModels() const {
    std::lock_guard<std::mutex> lk(models_mtx_);
    return open_models_;
}

void WakeWordEngine::SetDetectionCallback(DetectionCallback cb) {
    std::lock_guard<std::mutex> lk(cb_mtx_);
    on_detected_ = std::move(cb);
}

bool WakeWordEngine::Start() {
    if (running_.load(std::memory_order_relaxed)) return true;
    if (models_.empty() && open_models_.empty()) {
        LVA_LOGW(kTag, "Start: no wake-word models loaded; not starting");
        return false;
    }
    if (!models_.empty()) {
        features_ = std::make_unique<MicroFeatures>();
        if (!features_->Ok()) {
            LVA_LOGE(kTag, "Start: MicroFeatures init failed");
            features_.reset();
            return false;
        }
    }
    stop_requested_.store(false, std::memory_order_relaxed);
    running_.store(true, std::memory_order_relaxed);
    thread_ = std::thread([this] { ThreadLoop(); });
    LVA_LOGI(kTag, "started (%zu micro + %zu open model(s))",
             models_.size(), open_models_.size());
    return true;
}

void WakeWordEngine::Stop() {
    if (!running_.load(std::memory_order_relaxed)) return;
    stop_requested_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) thread_.join();
    running_.store(false, std::memory_order_relaxed);
    features_.reset();
}

void WakeWordEngine::ThreadLoop() {
    std::vector<std::int16_t> chunk(kPopSamples);
    std::vector<float> feature_buf;
    feature_buf.reserve(MicroFeatures::kFeatureSize * 16);
    std::vector<float> oww_embeddings;

    std::vector<std::shared_ptr<MicroWakeWord>> active_micro;
    std::vector<std::shared_ptr<OpenWakeWord>>  active_open;
    {
        std::lock_guard<std::mutex> lk(models_mtx_);
        active_micro = models_;
        active_open  = open_models_;
        models_changed_ = false;
    }

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        if (models_changed_) {
            std::lock_guard<std::mutex> lk(models_mtx_);
            active_micro = models_;
            active_open  = open_models_;
            models_changed_ = false;
            if (!active_micro.empty()) {
                features_ = std::make_unique<MicroFeatures>();
            } else {
                features_.reset();
            }
        }

        const std::size_t got = ring_.Read(chunk.data(), chunk.size());
        if (got == 0) {
            std::this_thread::sleep_for(kIdleSleep);
            continue;
        }

        // MicroWakeWord path
        if (!active_micro.empty() && features_) {
            feature_buf.clear();
            const std::size_t frames =
                features_->Process(chunk.data(), got, feature_buf);
            if (frames > 0) {
                for (auto& model : active_micro) {
                    float last_prob = 0.0f;
                    if (model->Process(feature_buf.data(), frames, &last_prob)) {
                        total_detections_.fetch_add(1, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lk(cb_mtx_);
                        if (on_detected_) {
                            try { on_detected_(model->config().id, last_prob); }
                            catch (...) {}
                        }
                    }
                }
            }
        }

        // OpenWakeWord path
        if (!active_open.empty() && open_features_) {
            oww_embeddings.clear();
            open_features_->Process(chunk.data(), got, oww_embeddings);
            if (!oww_embeddings.empty()) {
                for (auto& model : active_open) {
                    float last_prob = 0.0f;
                    if (model->Process(oww_embeddings.data(),
                                       oww_embeddings.size(), &last_prob)) {
                        total_detections_.fetch_add(1, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lk(cb_mtx_);
                        if (on_detected_) {
                            try { on_detected_(model->config().id, last_prob); }
                            catch (...) {}
                        }
                    }
                }
            }
        }
    }
}

}  // namespace lva::audio
