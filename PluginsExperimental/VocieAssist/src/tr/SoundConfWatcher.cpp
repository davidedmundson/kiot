#include "tr/SoundConfWatcher.h"

#include <cerrno>
#include <cmath>
#include <cstring>

#include "state/Preferences.h"
#include "state/ServerState.h"
#include "util/Log.h"

namespace lva::tr {

namespace {

constexpr const char* kTag = "soundwatch";

constexpr int kPollIntervalMs = 500;

}  // namespace

SoundConfWatcher::SoundConfWatcher(lva::state::ServerState& state,
                                   std::filesystem::path path)
    : state_(state), path_(std::move(path)) {
    struct stat st{};
    if (::stat(path_.c_str(), &st) == 0) {
        last_mtime_ = st.st_mtim;
    }
}

void SoundConfWatcher::Poll() {
    const auto now = std::chrono::steady_clock::now();
    if (last_check_.time_since_epoch().count() != 0) {
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_check_).count();
        if (elapsed_ms < kPollIntervalMs) return;
    }
    last_check_ = now;

    struct stat st{};
    if (::stat(path_.c_str(), &st) != 0) {
        // Silently ignore: file may not exist on first boot.
        return;
    }
    if (st.st_mtim.tv_sec  == last_mtime_.tv_sec &&
        st.st_mtim.tv_nsec == last_mtime_.tv_nsec) {
        return;  // unchanged
    }
    last_mtime_ = st.st_mtim;
    ReloadAndApply();
}

void SoundConfWatcher::ReloadAndApply() {
    auto fresh = lva::state::Preferences::LoadFromFile(path_);

    // ---- volume ----
    if (fresh.volume.has_value()) {
        const double on_disk = *fresh.volume;
        const double current =
            state_.volume.load(std::memory_order_relaxed);
        if (std::abs(on_disk - current) > 1e-4) {
            LVA_LOGI(kTag,
                     "external volume change detected: %.3f -> %.3f",
                     current, on_disk);
            state_.PersistVolume(on_disk);
        }
    }

    // ---- mic_mute ----
    const bool on_disk_muted  = fresh.is_mic_muted();
    const bool current_muted  = state_.muted.load(std::memory_order_relaxed);
    if (on_disk_muted != current_muted) {
        LVA_LOGI(kTag,
                 "external mic_mute change detected: %d -> %d",
                 current_muted ? 1 : 0, on_disk_muted ? 1 : 0);
        state_.PersistMuted(on_disk_muted);
        state_.PlayMuteToggleSound(on_disk_muted);
    }
}

}  // namespace lva::tr
