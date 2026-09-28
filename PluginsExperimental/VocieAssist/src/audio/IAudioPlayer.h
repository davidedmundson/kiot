
#pragma once

#include <functional>
#include <string>

namespace lva::audio {

enum class PlayerState {
    kIdle,
    kLoading,   // mpv is fetching/buffering the URL
    kPlaying,
    kPaused,
    kError,
};

class IAudioPlayer {
   public:
    virtual ~IAudioPlayer() = default;

    virtual void Play(const std::string& url,
                      std::function<void()> done_callback = {}) = 0;

    // Pause / resume. No-op if not currently playing.
    virtual void Pause()  = 0;
    virtual void Resume() = 0;

    // Stop playback and clear the playlist. Safe to call when idle.
    virtual void Stop() = 0;

    virtual void SetVolume(double normalized) = 0;

    // Current high-level state.
    virtual PlayerState State() const = 0;
};

}  // namespace lva::audio
