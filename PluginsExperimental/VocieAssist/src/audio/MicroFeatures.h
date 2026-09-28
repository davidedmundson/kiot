
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct FrontendState;
struct FrontendConfig;

namespace lva::audio {

class MicroFeatures {
   public:
    static constexpr unsigned kSampleRateHz       = 16'000;
    static constexpr unsigned kFeatureSize        = 40;
    static constexpr unsigned kFeatureStrideMs    = 10;
    static constexpr unsigned kFeatureWindowMs    = 30;
    static constexpr unsigned kSamplesPerChunk    =
        kFeatureStrideMs * (kSampleRateHz / 1000);  // 160
    static constexpr float    kFloat32Scale       = 1.0f / 25.6f;

    MicroFeatures();
    ~MicroFeatures();

    MicroFeatures(const MicroFeatures&)            = delete;
    MicroFeatures& operator=(const MicroFeatures&) = delete;

    // True if the frontend was successfully initialized.
    bool Ok() const noexcept { return state_ != nullptr; }

    std::size_t Process(const std::int16_t* samples,
                        std::size_t n,
                        std::vector<float>& out);

    void Reset();

   private:
    void ProcessOneChunk(const std::int16_t* chunk, std::vector<float>& out);

    FrontendState*  state_  = nullptr;
    FrontendConfig* config_ = nullptr;

    std::vector<std::int16_t> leftover_;
};

}  // namespace lva::audio
