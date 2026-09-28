#include "state/ServerState.h"

#include <algorithm>
#include <cmath>

#include "audio/IAudioPlayer.h"
#include "audio/MicroWakeWord.h"
#include "audio/OpenWakeWord.h"
#include "audio/WakeWordEngine.h"
#include "audio/WebRtcProcessor.h"
#include "entities/MediaPlayerEntity.h"
#include "entities/MuteSwitchEntity.h"
#include "satellite/Satellite.h"
#include "tr/LedRing.h"
#include "tr/MicMuteGpio.h"
#include "tr/SystemVolume.h"
#include "util/Log.h"

namespace lva::state {

namespace {
constexpr const char* kTag = "state";

double Clamp01(double v) noexcept {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}
}  // namespace

void ServerState::PersistVolume(double new_volume) {
    const double clamped = Clamp01(new_volume);
    const bool runtime_changed =
        std::abs(volume.load(std::memory_order_relaxed) - clamped) > 1e-4;
    const bool prefs_changed =
        !preferences.volume.has_value() ||
        std::abs(*preferences.volume - clamped) > 1e-4;
    if (!runtime_changed && !prefs_changed) {
        return;
    }
    volume.store(clamped, std::memory_order_relaxed);
    preferences.volume = clamped;
    lva::tr::SetSystemVolume(static_cast<int>(clamped * 100.0 + 0.5));
    lva::tr::ShowVolumeChanged();
    if (announce_player != nullptr) {
        announce_player->SetVolume(clamped);
    }
    if (media_player_entity != nullptr) {
        media_player_entity->BroadcastState();
    }
    if (!SavePreferences()) {
        LVA_LOGW(kTag, "PersistVolume(%.4f): save failed", clamped);
    }
}

void ServerState::PersistMuted(bool new_muted) {
    const bool runtime_changed =
        muted.load(std::memory_order_relaxed) != new_muted;
    const bool prefs_changed = preferences.is_mic_muted() != new_muted;
    if (!runtime_changed && !prefs_changed) return;

    muted.store(new_muted, std::memory_order_relaxed);
    preferences.set_mic_muted(new_muted);
    if (mic_mute_gpio != nullptr) {
        mic_mute_gpio->SyncToHardware(new_muted);
    }
    if (new_muted && satellite != nullptr) {
        satellite->OnMuted();
    }
    if (mute_switch_entity != nullptr) {
        mute_switch_entity->BroadcastState();
    }
    if (media_player_entity != nullptr) {
        media_player_entity->BroadcastState();
    }
    if (!SavePreferences()) {
        LVA_LOGW(kTag, "PersistMuted(%d): save failed", new_muted ? 1 : 0);
    }
}

void ServerState::PlayMuteToggleSound(bool muted) {
    if (announce_player == nullptr) return;
    announce_player->Play(muted
        ? "/usr/share/thirdreality/sounds/mute_switch_on.flac"
        : "/usr/share/thirdreality/sounds/mute_switch_off.flac",
        {});
}

void ServerState::PersistThinkingSound(bool enabled) {
    const int desired = enabled ? 1 : 0;
    if (preferences.thinking_sound == desired) return;
    preferences.thinking_sound = desired;
    if (!SavePreferences()) {
        LVA_LOGW(kTag, "PersistThinkingSound(%d): save failed", desired);
    }
}

void ServerState::PersistContinueConversationDelay(double seconds) {
    const double clamped = std::clamp(seconds, 0.0, 10.0);
    if (preferences.continue_conversation_delay.has_value() &&
        std::abs(*preferences.continue_conversation_delay - clamped) < 1e-4) {
        return;
    }
    preferences.continue_conversation_delay = clamped;
    continue_conversation_delay_ns =
        static_cast<std::int64_t>(clamped * 1e9);
    if (!SavePreferences()) {
        LVA_LOGW(kTag, "PersistContinueConversationDelay(%.3f): save failed",
                 clamped);
    }
}

void ServerState::PersistWakeWordSensitivity(int n, double value) {
    auto& slot = (n == 1) ? preferences.wake_word_1_sensitivity
                          : preferences.wake_word_2_sensitivity;
    if (slot.has_value() && std::abs(*slot - value) < 1e-4) return;
    slot = value;
    if (wakeword_engine) {
        const auto micro_models = wakeword_engine->SnapshotModels();
        const std::size_t target_idx = static_cast<std::size_t>(n - 1);
        std::size_t wake_idx = 0;
        bool applied = false;
        for (const auto& m : micro_models) {
            if (m->config().id == "stop") continue;
            if (wake_idx == target_idx) {
                m->SetProbabilityCutoff(static_cast<float>(value));
                applied = true;
                break;
            }
            ++wake_idx;
        }
        if (!applied) {
            const auto open_models = wakeword_engine->SnapshotOpenModels();
            for (const auto& m : open_models) {
                if (wake_idx == target_idx) {
                    m->SetProbabilityCutoff(static_cast<float>(value));
                    break;
                }
                ++wake_idx;
            }
        }
    }
    if (!SavePreferences()) {
        LVA_LOGW(kTag, "PersistWakeWordSensitivity(%d, %.4f): save failed",
                 n, value);
    }
}

void ServerState::PersistStopWordSensitivity(double value) {
    auto& slot = preferences.stop_word_sensitivity;
    if (slot.has_value() && std::abs(*slot - value) < 1e-4) return;
    slot = value;
    // Push to running stop model.
    if (wakeword_engine) {
        const auto micro_models = wakeword_engine->SnapshotModels();
        for (const auto& m : micro_models) {
            if (m->config().id == "stop") {
                m->SetProbabilityCutoff(static_cast<float>(value));
                break;
            }
        }
    }
    if (!SavePreferences()) {
        LVA_LOGW(kTag, "PersistStopWordSensitivity(%.4f): save failed", value);
    }
}

void ServerState::PersistMicAutoGain(int value) {
    const int clamped = std::clamp(value, 0, 31);
    if (preferences.mic_auto_gain == clamped) return;
    preferences.mic_auto_gain = clamped;
    if (webrtc_processor != nullptr) {
        webrtc_processor->SetLevels(clamped, preferences.mic_noise_suppression);
    }
    if (!SavePreferences()) {
        LVA_LOGW(kTag, "PersistMicAutoGain(%d): save failed", clamped);
    }
}

void ServerState::PersistMicNoiseSuppression(int value) {
    const int clamped = std::clamp(value, 0, 4);
    if (preferences.mic_noise_suppression == clamped) return;
    preferences.mic_noise_suppression = clamped;
    if (webrtc_processor != nullptr) {
        webrtc_processor->SetLevels(preferences.mic_auto_gain, clamped);
    }
    if (!SavePreferences()) {
        LVA_LOGW(kTag, "PersistMicNoiseSuppression(%d): save failed", clamped);
    }
}

void ServerState::PersistMicVolume(int value) {
    const int clamped = std::clamp(value, 1, 4000);
    if (preferences.mic_volume == clamped) return;
    preferences.mic_volume = clamped;
    mic_volume_live.store(clamped, std::memory_order_relaxed);
    if (!SavePreferences()) {
        LVA_LOGW(kTag, "PersistMicVolume(%d): save failed", clamped);
    }
}

}  // namespace lva::state
