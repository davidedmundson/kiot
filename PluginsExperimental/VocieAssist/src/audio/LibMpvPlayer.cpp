#include "audio/LibMpvPlayer.h"

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>

#include <mpv/client.h>

#include "util/Log.h"

namespace lva::audio {

namespace {

constexpr const char* kTag = "mpv";

double Clamp01(double v) noexcept {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

void SetMpvOptionString(mpv_handle* mpv, const char* name, const char* value) {
    const int rc = mpv_set_option_string(mpv, name, value);
    if (rc < 0) {
        LVA_LOGW(kTag, "set_option_string(%s=%s) failed: %s",
                 name, value, mpv_error_string(rc));
    }
}

}  // namespace

LibMpvPlayer::LibMpvPlayer(const Options& opts) {
    mpv_ = mpv_create();
    if (mpv_ == nullptr) {
        LVA_LOGE(kTag, "mpv_create returned NULL — libmpv missing or failing");
        return;
    }

    // No video/display, no terminal control. We're a daemon.
    SetMpvOptionString(mpv_, "audio-display", "no");
    SetMpvOptionString(mpv_, "vid",            "no");
    SetMpvOptionString(mpv_, "video",          "no");
    SetMpvOptionString(mpv_, "input-default-bindings", "no");
    SetMpvOptionString(mpv_, "input-vo-keyboard",      "no");
    SetMpvOptionString(mpv_, "terminal", "no");

    // Network playback cache. Matches the python-mpv build's tunings.
    SetMpvOptionString(mpv_, "cache",                "yes");
    SetMpvOptionString(mpv_, "cache-pause",          "yes");
    SetMpvOptionString(mpv_, "cache-pause-wait",     "10");
    SetMpvOptionString(mpv_, "demuxer-max-bytes",    "2MiB");
    SetMpvOptionString(mpv_, "demuxer-max-back-bytes", "512KiB");
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", opts.cache_secs);
        SetMpvOptionString(mpv_, "cache-secs", buf);
    }

    SetMpvOptionString(mpv_, "audio-stream-silence", "yes");

    if (opts.short_sound_safe) {
        SetMpvOptionString(mpv_, "audio-buffer", "0.8");
    }

    if (!opts.audio_device.empty()) {
        SetMpvOptionString(mpv_, "audio-device", opts.audio_device.c_str());
    }

    SetMpvOptionString(mpv_, "msg-level", "all=warn");

    if (const int rc = mpv_initialize(mpv_); rc < 0) {
        LVA_LOGE(kTag, "mpv_initialize failed: %s", mpv_error_string(rc));
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
        return;
    }

    const int efd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (efd < 0) {
        LVA_LOGE(kTag, "eventfd: %s", std::strerror(errno));
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
        return;
    }
    wakeup_read_fd_  = efd;
    wakeup_write_fd_ = efd;  // eventfd is r/w from same fd

    mpv_set_wakeup_callback(mpv_, &OnMpvWakeupTrampoline, this);

    DrainEvents();

    LVA_LOGI(kTag, "libmpv initialized (cache_secs=%d device=%s)",
             opts.cache_secs,
             opts.audio_device.empty() ? "default" : opts.audio_device.c_str());
}

LibMpvPlayer::~LibMpvPlayer() {
    if (mpv_ != nullptr) {
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
    }
    if (wakeup_read_fd_ >= 0) {
        ::close(wakeup_read_fd_);
        wakeup_read_fd_  = -1;
        wakeup_write_fd_ = -1;
    }
}

void LibMpvPlayer::OnMpvWakeupTrampoline(void* opaque) {
    auto* self = static_cast<LibMpvPlayer*>(opaque);
    if (self == nullptr || self->wakeup_write_fd_ < 0) return;
    const std::uint64_t value = 1;
    ssize_t r;
    do {
        r = ::write(self->wakeup_write_fd_, &value, sizeof(value));
    } while (r < 0 && errno == EINTR);
    (void)r;  // best-effort
}

void LibMpvPlayer::ReadDrainWakeupFd() {
    std::uint64_t v = 0;
    while (::read(wakeup_read_fd_, &v, sizeof(v)) > 0) {
        // drain
    }
}

void LibMpvPlayer::DrainEvents() {
    if (mpv_ == nullptr) return;
    ReadDrainWakeupFd();

    auto set_state = [this](PlayerState new_state) {
        const PlayerState prev =
            state_.exchange(new_state, std::memory_order_relaxed);
        if (prev != new_state && on_state_changed_) {
            try {
                on_state_changed_(new_state);
            } catch (...) {
                // Listener errors must never break the loop.
            }
        }
    };

    while (true) {
        mpv_event* ev = mpv_wait_event(mpv_, 0);
        if (ev == nullptr || ev->event_id == MPV_EVENT_NONE) {
            break;
        }
        switch (ev->event_id) {
            case MPV_EVENT_START_FILE:
                set_state(PlayerState::kLoading);
                LVA_LOGD(kTag, "event: START_FILE");
                break;
            case MPV_EVENT_FILE_LOADED:
                set_state(PlayerState::kPlaying);
                LVA_LOGD(kTag, "event: FILE_LOADED");
                break;
            case MPV_EVENT_END_FILE: {
                auto* end = static_cast<mpv_event_end_file*>(ev->data);
                const int reason = end ? static_cast<int>(end->reason) : -1;
                LVA_LOGD(kTag, "event: END_FILE (reason=%d)", reason);
                set_state(PlayerState::kIdle);
                if (reason == MPV_END_FILE_REASON_EOF ||
                    reason == MPV_END_FILE_REASON_ERROR) {
                    std::function<void()> cb;
                    {
                        std::lock_guard<std::mutex> lk(cb_mutex_);
                        cb = std::move(done_callback_);
                        done_callback_ = nullptr;
                    }
                    if (cb) {
                        try {
                            cb();
                        } catch (...) {
                        }
                    }
                }
                break;
            }
            case MPV_EVENT_LOG_MESSAGE: {
                auto* m = static_cast<mpv_event_log_message*>(ev->data);
                if (m && m->level && m->text) {
                    LVA_LOGD(kTag, "[mpv:%s] %s", m->level, m->text);
                }
                break;
            }
            case MPV_EVENT_SHUTDOWN:
                LVA_LOGI(kTag, "event: SHUTDOWN");
                set_state(PlayerState::kIdle);
                return;
            default:
                LVA_LOGD(kTag, "event: %s", mpv_event_name(ev->event_id));
                break;
        }
    }
}

void LibMpvPlayer::Play(const std::string& url,
                        std::function<void()> done_callback) {
    if (mpv_ == nullptr) {
        LVA_LOGW(kTag, "Play(%s) ignored: mpv not initialized",
                 url.c_str());
        return;
    }
    {
        std::lock_guard<std::mutex> lk(cb_mutex_);
        done_callback_ = std::move(done_callback);
    }

    const char* args[] = {"loadfile", url.c_str(), "replace", nullptr};
    if (const int rc = mpv_command(mpv_, args); rc < 0) {
        LVA_LOGE(kTag, "loadfile(%s) failed: %s", url.c_str(),
                 mpv_error_string(rc));
        std::lock_guard<std::mutex> lk(cb_mutex_);
        done_callback_ = nullptr;
        return;
    }
    state_.store(PlayerState::kLoading, std::memory_order_relaxed);
    LVA_LOGI(kTag, "Play: %s", url.c_str());
}

void LibMpvPlayer::Pause() {
    if (mpv_ == nullptr) return;
    int yes = 1;
    if (const int rc = mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &yes);
        rc < 0) {
        LVA_LOGW(kTag, "pause set failed: %s", mpv_error_string(rc));
        return;
    }
    state_.store(PlayerState::kPaused, std::memory_order_relaxed);
}

void LibMpvPlayer::Resume() {
    if (mpv_ == nullptr) return;
    int no = 0;
    if (const int rc = mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &no);
        rc < 0) {
        LVA_LOGW(kTag, "resume set failed: %s", mpv_error_string(rc));
        return;
    }
    state_.store(PlayerState::kPlaying, std::memory_order_relaxed);
}

void LibMpvPlayer::Stop() {
    if (mpv_ == nullptr) return;
    {
        std::lock_guard<std::mutex> lk(cb_mutex_);
        done_callback_ = nullptr;
    }
    const char* args[] = {"stop", nullptr};
    if (const int rc = mpv_command(mpv_, args); rc < 0) {
        LVA_LOGW(kTag, "stop failed: %s", mpv_error_string(rc));
        return;
    }
    state_.store(PlayerState::kIdle, std::memory_order_relaxed);
}

void LibMpvPlayer::SetVolume(double normalized) {
    user_volume_ = Clamp01(normalized);
    if (mpv_ == nullptr) return;
    // Use sqrt curve to match sendspin's software attenuation.
    // This gives a gentler rolloff: 50% → ~71% output.
    double percent = std::sqrt(user_volume_) * 100.0;
    if (const int rc = mpv_set_property(mpv_, "volume",
                                        MPV_FORMAT_DOUBLE, &percent);
        rc < 0) {
        LVA_LOGW(kTag, "volume set failed: %s", mpv_error_string(rc));
    }
}

PlayerState LibMpvPlayer::State() const {
    return state_.load(std::memory_order_relaxed);
}

void LibMpvPlayer::SetStateChangedCallback(
    std::function<void(PlayerState)> on_state_changed) {
    on_state_changed_ = std::move(on_state_changed);
}

}  // namespace lva::audio
