
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include "audio/IAudioPlayer.h"

struct mpv_handle;

namespace lva::audio {

class LibMpvPlayer final : public IAudioPlayer {
   public:
    struct Options {
        // PulseAudio sink to send to. Empty = mpv's default.
        std::string audio_device;
        int  cache_secs       = 2;
        bool short_sound_safe = true;
    };

    explicit LibMpvPlayer(const Options& opts);
    ~LibMpvPlayer() override;

    LibMpvPlayer(const LibMpvPlayer&)            = delete;
    LibMpvPlayer& operator=(const LibMpvPlayer&) = delete;

    // IAudioPlayer
    void        Play(const std::string& url,
                     std::function<void()> done_callback = {}) override;
    void        Pause()  override;
    void        Resume() override;
    void        Stop()   override;
    void        SetVolume(double normalized) override;
    PlayerState State() const override;

    int  WakeupFd() const noexcept { return wakeup_read_fd_; }

    void DrainEvents();

    void SetStateChangedCallback(
        std::function<void(PlayerState)> on_state_changed);

   private:
    static void OnMpvWakeupTrampoline(void* opaque);
    void        ReadDrainWakeupFd();

    mpv_handle* mpv_      = nullptr;
    int  wakeup_read_fd_  = -1;
    int  wakeup_write_fd_ = -1;

    // Updated in DrainEvents() based on mpv events.
    std::atomic<PlayerState> state_{PlayerState::kIdle};

    std::function<void(PlayerState)> on_state_changed_;

    std::mutex                   cb_mutex_;
    std::function<void()>        done_callback_;

    double user_volume_ = 1.0;
};

}  // namespace lva::audio
