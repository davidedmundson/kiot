
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/message_lite.h>

#include "entities/Entity.h"
#include "state/Preferences.h"

namespace lva::audio    { class AudioCapture; class WebRtcProcessor; class IAudioPlayer; class WakeWordEngine; }
namespace lva::satellite { class Satellite; }
namespace lva::tr       { class MicMuteGpio; class Supervisor; }
namespace lva::entities { class MediaPlayerEntity; class MuteSwitchEntity; }

namespace lva::state {

class ServerState {
   public:
    // ---- identity (set at startup, never changes) ----
    std::string name;            // e.g. "3RSPK-A8E291152F53"
    std::string friendly_name;   // human-readable fallback to name
    std::string mac_address;     // colon-separated, lowercase
    std::string version;         // our project version
    std::string esphome_version; // pinned to api.proto's contract

    // ---- preferences (file-backed, hot path mirrors below) ----
    Preferences preferences;
    std::filesystem::path preferences_path;
    std::filesystem::path wakeword_dir;

    std::atomic<bool>   muted{false};
    std::atomic<double> volume{1.0};
    std::atomic<int>    mic_volume_live{100}; // 1-4000 (percent), read by capture thread

    std::int64_t continue_conversation_delay_ns = 500'000'000LL;

    class ::lva::audio::AudioCapture* audio_capture = nullptr;

    class ::lva::satellite::Satellite* satellite = nullptr;

    class ::lva::audio::WebRtcProcessor* webrtc_processor = nullptr;

    // Wake-word engine — used to push sensitivity changes at runtime.
    class ::lva::audio::WakeWordEngine* wakeword_engine = nullptr;

    class ::lva::tr::MicMuteGpio* mic_mute_gpio = nullptr;

    class ::lva::entities::MediaPlayerEntity* media_player_entity = nullptr;

    class ::lva::entities::MuteSwitchEntity* mute_switch_entity = nullptr;

    class ::lva::audio::IAudioPlayer* announce_player = nullptr;

    // Music player — used by Satellite to duck volume during voice.
    class ::lva::audio::IAudioPlayer* music_player = nullptr;

    class ::lva::tr::Supervisor* supervisor = nullptr;

    using BroadcastFn = std::function<void(
        std::uint32_t msg_type_id,
        const ::google::protobuf::MessageLite& msg)>;
    BroadcastFn broadcast;

    using ConnectionEventFn = std::function<void()>;
    ConnectionEventFn on_client_authenticated;

    ConnectionEventFn on_client_disconnected;

    std::vector<std::unique_ptr<entities::Entity>> entities;

    bool SavePreferences() const {
        return preferences.SaveToFile(preferences_path);
    }

    void PersistVolume(double new_volume);

    // Bump mute state similarly.
    void PersistMuted(bool new_muted);

    void PlayMuteToggleSound(bool muted);

    // Bump thinking_sound similarly (0/1).
    void PersistThinkingSound(bool enabled);

    // Persist the continued-conversation listen delay (seconds) and
    // apply it to the live satellite timing.
    void PersistContinueConversationDelay(double seconds);

    // Bump wake_word_N_sensitivity (1-based n in {1, 2}).
    void PersistWakeWordSensitivity(int n, double value);
    void PersistStopWordSensitivity(double value);

    // Bump mic_* values. Each clamps to its valid range.
    void PersistMicAutoGain(int value);
    void PersistMicNoiseSuppression(int value);
    void PersistMicVolume(int value);
};

}  // namespace lva::state
