#include "tr/MicMuteGpio.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>

#include "state/ServerState.h"
#include "tr/LedRing.h"
#include "util/Log.h"

namespace lva::tr {

namespace {

constexpr const char* kTag             = "mute_gpio";
constexpr int    kPollIntervalMs       = 500;

bool GpioToMuted(int gpio_value) noexcept {
    return gpio_value == 0;
}

int MutedToGpio(bool muted) noexcept {
    return muted ? 0 : 1;
}

}  // namespace

MicMuteGpio::MicMuteGpio(lva::state::ServerState& state,
                         std::string gpio_path)
    : state_(state), gpio_path_(std::move(gpio_path)) {
    struct stat st{};
    if (::stat(gpio_path_.c_str(), &st) == 0) {
        available_ = true;
        LVA_LOGI(kTag, "available at %s", gpio_path_.c_str());
    } else {
        LVA_LOGW(kTag, "GPIO not available at %s; mic mute is software-only",
                 gpio_path_.c_str());
    }
}

bool MicMuteGpio::ReadRaw(int* out_value) {
    std::ifstream f(gpio_path_);
    if (!f.is_open()) return false;
    int v = -1;
    f >> v;
    if (!f) return false;
    if (v != 0 && v != 1) return false;
    *out_value = v;
    return true;
}

bool MicMuteGpio::ReadAndApplyOnce() {
    if (!available_) return false;
    int v = -1;
    if (!ReadRaw(&v)) return false;
    last_value_ = v;
    const bool muted = GpioToMuted(v);
    state_.PersistMuted(muted);
    return true;
}

void MicMuteGpio::Poll() {
    if (!available_) return;
    const auto now = std::chrono::steady_clock::now();
    if (last_poll_.time_since_epoch().count() != 0) {
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_poll_).count();
        if (elapsed_ms < kPollIntervalMs) return;
    }
    last_poll_ = now;

    int v = -1;
    if (!ReadRaw(&v)) return;
    if (v == last_value_) return;
    last_value_ = v;
    const bool muted = GpioToMuted(v);
    LVA_LOGI(kTag, "hardware change: muted=%s", muted ? "true" : "false");
    state_.PersistMuted(muted);
    state_.PlayMuteToggleSound(muted);
    Show(muted ? LedState::Muted : LedState::Unmuted);
}

void MicMuteGpio::SyncToHardware(bool muted) {
    if (!available_) return;
    const int target = MutedToGpio(muted);
    if (target == last_value_) return;  // already matches

    std::ofstream f(gpio_path_);
    if (!f.is_open()) {
        LVA_LOGW(kTag, "open(%s) for write failed: %s",
                 gpio_path_.c_str(), std::strerror(errno));
        return;
    }
    f << target;
    if (!f) {
        LVA_LOGW(kTag, "write to %s failed", gpio_path_.c_str());
        return;
    }
    last_value_ = target;
    LVA_LOGI(kTag, "GPIO -> %d (muted=%s)", target,
             muted ? "true" : "false");
    Show(muted ? LedState::Muted : LedState::Unmuted);
}

}  // namespace lva::tr
