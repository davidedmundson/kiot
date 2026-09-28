
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace lva::audio {

class MicroFeatures;
class MicroWakeWord;
class OpenWakeWord;
class OpenWakeWordFeatures;
class PcmRingBuffer;

class WakeWordEngine {
   public:
    using DetectionCallback =
        std::function<void(const std::string& model_id, float prob)>;

    explicit WakeWordEngine(PcmRingBuffer& ring);
    ~WakeWordEngine();

    WakeWordEngine(const WakeWordEngine&)            = delete;
    WakeWordEngine& operator=(const WakeWordEngine&) = delete;

    void AddModel(std::shared_ptr<MicroWakeWord> model);
    void AddOpenModel(std::shared_ptr<OpenWakeWord> model);
    void SetOpenFeatures(std::unique_ptr<OpenWakeWordFeatures> features);

    void SetActiveModels(std::vector<std::shared_ptr<MicroWakeWord>> new_models);
    void SetActiveOpenModels(std::vector<std::shared_ptr<OpenWakeWord>> new_models);

    void SetDetectionCallback(DetectionCallback cb);

    bool Start();
    void Stop();
    void RequestStop() noexcept { stop_requested_.store(true, std::memory_order_relaxed); }

    std::uint64_t TotalDetections() const noexcept {
        return total_detections_.load(std::memory_order_relaxed);
    }

    std::vector<std::shared_ptr<MicroWakeWord>> SnapshotModels() const;
    std::vector<std::shared_ptr<OpenWakeWord>>  SnapshotOpenModels() const;

    const std::vector<std::shared_ptr<MicroWakeWord>>& Models() const noexcept {
        return models_;
    }
    const std::vector<std::shared_ptr<OpenWakeWord>>& OpenModels() const noexcept {
        return open_models_;
    }

   private:
    void ThreadLoop();

    PcmRingBuffer&  ring_;
    std::unique_ptr<MicroFeatures> features_;
    std::unique_ptr<OpenWakeWordFeatures> open_features_;
    std::vector<std::shared_ptr<MicroWakeWord>> models_;
    std::vector<std::shared_ptr<OpenWakeWord>>  open_models_;
    mutable std::mutex models_mtx_;
    std::atomic<bool>  models_changed_{false};

    DetectionCallback on_detected_;
    std::mutex        cb_mtx_;

    std::thread       thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<std::uint64_t> total_detections_{0};
};

}  // namespace lva::audio
