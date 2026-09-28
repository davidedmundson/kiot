#include "satellite/Satellite.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

#include "audio/ExternalWakeWord.h"
#include "audio/IAudioPlayer.h"
#include "audio/MicroWakeWord.h"
#include "audio/OpenWakeWord.h"
#include "audio/PcmRingBuffer.h"
#include "audio/WakeWordEngine.h"
#include "audio/WakeWordScanner.h"
#include "protocol/MessageRegistry.h"
#include "state/ServerState.h"
#include "tr/LedRing.h"
#include "tr/SendspinSignal.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::satellite {

namespace {

constexpr const char* kTag = "satellite";

constexpr std::size_t kAudioPumpSamples = 1024;

constexpr double kDuckFactor = 0.2;

constexpr int kMaxPumpsPerTick = 4;

constexpr int kEvtError          = 0;
constexpr int kEvtRunStart       = 1;
constexpr int kEvtRunEnd         = 2;
constexpr int kEvtSttEnd         = 4;
constexpr int kEvtIntentStart    = 5;
constexpr int kEvtIntentEnd      = 6;
constexpr int kEvtSttVadEnd      = 12;
constexpr int kEvtIntentProgress = 100;
constexpr int kEvtTtsEnd         = 8;

constexpr const char* kThinkingSoundPath =
    "/usr/share/thirdreality/sounds/processing.wav";

constexpr const char* kWakeupSoundPath =
    "/usr/share/thirdreality/sounds/wake_word_triggered.flac";

constexpr const char* kTimerFinishedSoundPath =
    "/usr/share/thirdreality/sounds/timer_finished.flac";

constexpr int kTimerMaxRingSeconds = 900;

}  // namespace

Satellite::Satellite(lva::state::ServerState& state,
                     lva::audio::PcmRingBuffer& mic_ring,
                     lva::audio::WakeWordEngine& engine,
                     lva::audio::IAudioPlayer* announce_player)
    : state_(state), mic_ring_(mic_ring), engine_(engine),
      announce_player_(announce_player) {}

Satellite::~Satellite() = default;

void Satellite::OnWakeDetected(const std::string& model_id, float prob) {
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now_ns < refractory_until_ns_.load(std::memory_order_relaxed)) {
        return;  // fast-path drop on ww-engine thread
    }
    pending_wake_data_.model_id = model_id;
    pending_wake_data_.prob     = prob;
    pending_wake_.store(true, std::memory_order_release);
}

void Satellite::OnStopDetected() {
    if (!pipeline_active_ && !timer_ringing_) return;

    if (timer_ringing_) {
        LVA_LOGI(kTag, "stop word → stopping timer ring");
        StopTimerRing();
    } else if (announce_player_) {
        LVA_LOGI(kTag, "stop word → stopping TTS");
        announce_player_->Stop();
        pipeline_active_       = false;
        continue_conversation_ = false;
        continue_listen_at_ns_.store(0, std::memory_order_relaxed);
        Unduck();
        lva::tr::Show(lva::tr::LedState::Idle);
    }
}

void Satellite::StartPipeline(const std::string& wake_word_phrase) {
    const auto now = std::chrono::steady_clock::now();
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    if (now_ns < refractory_until_ns_.load(std::memory_order_relaxed)) {
        const auto remaining_ms =
            (refractory_until_ns_.load(std::memory_order_relaxed) - now_ns)
            / 1'000'000;
        LVA_LOGD(kTag, "wake suppressed (refractory %lld ms remaining)",
                 static_cast<long long>(remaining_ms));
        return;
    }
    if (pipeline_active_) {
        LVA_LOGD(kTag, "wake during active pipeline; ignoring");
        return;
    }
    if (timer_ringing_) {
        LVA_LOGI(kTag, "wake during timer ring — stopping ring");
        StopTimerRing();
        refractory_until_ns_.store(now_ns + 2'000'000'000LL,
                                   std::memory_order_relaxed);
        return;
    }
    LVA_LOGI(kTag, "starting voice pipeline (wake='%s')",
             wake_word_phrase.c_str());

    // Refractory window mirrors Python LVA's `refractory_seconds=2.0`.
    refractory_until_ns_.store(now_ns + 2'000'000'000LL,
                               std::memory_order_relaxed);

    pipeline_active_       = true;
    is_streaming_audio_    = false;  // not yet — wakeup sound first
    tts_url_.clear();
    tts_played_            = false;
    continue_conversation_ = false;

    lva::tr::Show(lva::tr::LedState::Listening);
    Duck();

    if (announce_player_ != nullptr) {
        const std::string phrase_copy = wake_word_phrase;
        announce_player_->Play(kWakeupSoundPath, [this, phrase_copy] {
            OpenStreamToHa(phrase_copy);
        });
    } else {
        OpenStreamToHa(wake_word_phrase);
    }
}

void Satellite::OpenStreamToHa(const std::string& wake_word_phrase) {
    if (!pipeline_active_) {
        LVA_LOGD(kTag, "OpenStreamToHa: pipeline no longer active; skip");
        return;
    }
    is_streaming_audio_ = true;

    {
        std::int16_t scratch[1024];
        std::size_t total_drained = 0;
        while (true) {
            const std::size_t got = mic_ring_.Read(scratch,
                                                   sizeof(scratch) /
                                                   sizeof(scratch[0]));
            if (got == 0) break;
            total_drained += got;
        }
        if (total_drained > 0) {
            LVA_LOGD(kTag, "drained %zu samples (%.0f ms) before stream",
                     total_drained,
                     total_drained * 1000.0 / 16000.0);
        }
    }

    if (!state_.broadcast) {
        LVA_LOGW(kTag, "no broadcast hook; can't notify HA");
        return;
    }
    ::VoiceAssistantRequest req;
    req.set_start(true);
    req.set_wake_word_phrase(wake_word_phrase);
    state_.broadcast(lva::proto::kIdVoiceAssistantRequest, req);
}

void Satellite::PumpAudioToHa() {
    if (!is_streaming_audio_) return;
    if (!state_.broadcast)    return;

    std::vector<std::int16_t> chunk(kAudioPumpSamples);
    for (int i = 0; i < kMaxPumpsPerTick; ++i) {
        const std::size_t got =
            mic_ring_.Read(chunk.data(), chunk.size());
        if (got == 0) break;

        ::VoiceAssistantAudio audio_msg;
        audio_msg.set_data(reinterpret_cast<const char*>(chunk.data()),
                           got * sizeof(std::int16_t));
        state_.broadcast(lva::proto::kIdVoiceAssistantAudio, audio_msg);
    }
}

void Satellite::StopAudioStreaming() {
    if (!is_streaming_audio_) return;
    is_streaming_audio_ = false;
    LVA_LOGD(kTag, "stopped streaming audio to HA");
}

void Satellite::BeginListening() {
    // Drain stale mic audio captured during TTS playback before re-streaming.
    {
        std::int16_t scratch[1024];
        while (mic_ring_.Read(scratch, 1024) > 0) {}
    }
    lva::tr::Show(lva::tr::LedState::Listening);
    is_streaming_audio_ = true;
    pipeline_active_    = true;
    if (state_.broadcast) {
        ::VoiceAssistantRequest req;
        req.set_start(true);
        state_.broadcast(lva::proto::kIdVoiceAssistantRequest, req);
    }
}

void Satellite::PlayTts() {
    if (tts_url_.empty()) {
        LVA_LOGW(kTag, "PlayTts called with empty url");
        return;
    }
    if (tts_played_) {
        LVA_LOGD(kTag, "PlayTts: already played; skip");
        return;
    }
    if (announce_player_ == nullptr) {
        LVA_LOGW(kTag, "PlayTts: no announce player");
        return;
    }

    tts_played_ = true;
    LVA_LOGI(kTag, "playing TTS: %s", tts_url_.c_str());
    lva::tr::Show(lva::tr::LedState::Speaking);

    auto broadcast = state_.broadcast;
    announce_player_->Play(tts_url_, [this, broadcast]() {
        if (broadcast) {
            ::VoiceAssistantAnnounceFinished done;
            broadcast(lva::proto::kIdVoiceAssistantAnnounceFinished, done);
        }
        if (continue_conversation_) {
            LVA_LOGI(kTag, "TTS done — continuing conversation after %lld ms",
                     static_cast<long long>(
                         state_.continue_conversation_delay_ns / 1'000'000));
            // TTS has finished playing; show Thinking during the settle delay
            // instead of leaving the LED on Speaking. BeginListening() will
            // switch to Listening once the mic actually opens.
            lva::tr::Show(lva::tr::LedState::Thinking);
            const auto now_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
            continue_listen_at_ns_.store(
                now_ns + state_.continue_conversation_delay_ns,
                std::memory_order_release);
        } else {
            pipeline_active_ = false;
            lva::tr::Show(lva::tr::LedState::Idle);
            Unduck();
        }
    });
}

void Satellite::PlayAnnounce(const std::string& media_id,
                             const std::string& preannounce_media_id,
                             bool start_conversation) {
    if (announce_player_ == nullptr) return;
    if (media_id.empty()) return;

    pipeline_active_ = true;
    Duck();
    continue_conversation_ = start_conversation;

    auto broadcast = state_.broadcast;
    auto play_main = [this, media_id, broadcast]() {
        announce_player_->Play(media_id, [this, broadcast]() {
            if (broadcast) {
                ::VoiceAssistantAnnounceFinished done;
                done.set_success(true);
                broadcast(lva::proto::kIdVoiceAssistantAnnounceFinished, done);
            }
            if (continue_conversation_) {
                LVA_LOGI(kTag, "announce done — continuing conversation "
                               "after %lld ms",
                         static_cast<long long>(
                             state_.continue_conversation_delay_ns
                             / 1'000'000));
                // Show Thinking during the settle delay; BeginListening()
                // switches to Listening when the mic opens.
                lva::tr::Show(lva::tr::LedState::Thinking);
                const auto now_ns =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
                continue_listen_at_ns_.store(
                    now_ns + state_.continue_conversation_delay_ns,
                    std::memory_order_release);
            } else {
                pipeline_active_ = false;
                lva::tr::Show(lva::tr::LedState::Idle);
                Unduck();
            }
        });
    };

    if (!preannounce_media_id.empty()) {
        announce_player_->Play(preannounce_media_id,
                               std::move(play_main));
    } else {
        play_main();
    }
}

void Satellite::Duck() {
    if (ducked_) return;
    ducked_ = true;
    lva::tr::SendspinDuck();
    if (state_.music_player) {
        pre_duck_volume_ = state_.volume.load(std::memory_order_relaxed);
        state_.music_player->SetVolume(pre_duck_volume_ * kDuckFactor);
    }
}

void Satellite::Unduck() {
    if (!ducked_) return;
    ducked_ = false;
    lva::tr::SendspinUnduck();
    if (state_.music_player) {
        state_.music_player->SetVolume(pre_duck_volume_);
    }
}

void Satellite::StartTimerRing() {
    if (timer_ringing_) return;  // already ringing
    timer_ringing_    = true;
    timer_ring_start_ = std::chrono::steady_clock::now();
    if (announce_player_ != nullptr) {
        announce_player_->Play(kTimerFinishedSoundPath, [this] {
            OnTimerSoundEof();
        });
    }
    Duck();
    lva::tr::Show(lva::tr::LedState::Speaking);
}

void Satellite::StopTimerRing() {
    if (!timer_ringing_) return;
    timer_ringing_ = false;
    if (announce_player_ != nullptr) {
        announce_player_->Stop();
    }
    Unduck();
    lva::tr::Show(lva::tr::LedState::Idle);
}

void Satellite::OnTimerSoundEof() {
    if (!timer_ringing_) return;  // user cancelled

    // Auto-stop after kTimerMaxRingSeconds.
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - timer_ring_start_).count();
    if (elapsed >= kTimerMaxRingSeconds) {
        LVA_LOGI(kTag, "timer ring auto-stopped after %lld s",
                 static_cast<long long>(elapsed));
        StopTimerRing();
        return;
    }
    if (announce_player_ != nullptr) {
        announce_player_->Play(kTimerFinishedSoundPath, [this] {
            OnTimerSoundEof();
        });
    }
}

void Satellite::OnVoiceEvent(
    int event_type,
    const std::vector<std::pair<std::string, std::string>>& data) {
    switch (event_type) {
        case kEvtRunStart:
            LVA_LOGD(kTag, "RUN_START");
            // Some pipelines deliver a TTS URL here; capture if present.
            for (const auto& [k, v] : data) {
                if (k == "url") tts_url_ = v;
            }
            tts_played_           = false;
            continue_conversation_ = false;
            break;

        case kEvtError: {
            std::string code, message;
            for (const auto& [k, v] : data) {
                if      (k == "code")    code    = v;
                else if (k == "message") message = v;
            }
            LVA_LOGW(kTag, "voice ERROR event: code=%s message=%s",
                     code.empty()    ? "(none)" : code.c_str(),
                     message.empty() ? "(none)" : message.c_str());
            lva::tr::Show(lva::tr::LedState::Error);
            StopAudioStreaming();
            pipeline_active_       = false;
            continue_conversation_ = false;
            // Pair the duck() from StartPipeline.
            Unduck();
            break;
        }

        case kEvtSttVadEnd:
        case kEvtSttEnd:
            LVA_LOGD(kTag, "STT_(VAD_)END — stop streaming");
            StopAudioStreaming();
            lva::tr::Show(lva::tr::LedState::Thinking);
            break;

        case kEvtIntentStart:
            LVA_LOGD(kTag, "INTENT_START");
            if (state_.preferences.thinking_sound != 0 &&
                announce_player_ != nullptr) {
                LVA_LOGI(kTag, "playing thinking sound");
                announce_player_->Play(kThinkingSoundPath, {});
            }
            break;

        case kEvtIntentEnd:
            for (const auto& [k, v] : data) {
                if (k == "continue_conversation" && v == "1") {
                    continue_conversation_ = true;
                }
            }
            LVA_LOGD(kTag, "INTENT_END (continue=%d)",
                     continue_conversation_ ? 1 : 0);
            break;

        case kEvtIntentProgress:
            for (const auto& [k, v] : data) {
                if (k == "tts_start_streaming" && v == "1") {
                    LVA_LOGD(kTag, "INTENT_PROGRESS: early TTS start");
                    PlayTts();
                }
            }
            break;

        case kEvtTtsEnd:
            for (const auto& [k, v] : data) {
                if (k == "url") tts_url_ = v;
            }
            LVA_LOGI(kTag, "TTS_END — tts_url=%s", tts_url_.c_str());
            PlayTts();
            break;

        case kEvtRunEnd:
            for (const auto& [k, v] : data) {
                if (k == "continue_conversation" && v == "1") {
                    continue_conversation_ = true;
                }
            }
            LVA_LOGI(kTag, "RUN_END (continue=%d tts_played=%d)",
                     continue_conversation_ ? 1 : 0,
                     tts_played_ ? 1 : 0);
            StopAudioStreaming();
            if (!tts_played_) {
                if (continue_conversation_) {
                    lva::tr::Show(lva::tr::LedState::Listening);
                } else {
                    pipeline_active_ = false;
                    lva::tr::Show(lva::tr::LedState::Idle);
                    Unduck();
                }
            }
            break;

        default:
            LVA_LOGD(kTag, "voice event %d", event_type);
            break;
    }
}

void Satellite::OnLoopTick() {
    // Consume pending wake-word event, if any.
    if (pending_wake_.load(std::memory_order_acquire)) {
        const PendingWake snap = pending_wake_data_;
        pending_wake_.store(false, std::memory_order_relaxed);
        StartPipeline(snap.model_id);
    }

    // Open the mic for a continued conversation once the settle delay elapses.
    const auto listen_at = continue_listen_at_ns_.load(std::memory_order_acquire);
    if (listen_at != 0) {
        const auto now_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now_ns >= listen_at) {
            continue_listen_at_ns_.store(0, std::memory_order_relaxed);
            if (state_.muted.load(std::memory_order_relaxed)) {
                LVA_LOGD(kTag, "continued conversation skipped: muted");
                pipeline_active_ = false;
                lva::tr::Show(lva::tr::LedState::Idle);
                Unduck();
            } else {
                LVA_LOGI(kTag, "settle delay elapsed — listening");
                BeginListening();
            }
        }
    }

    // Pump mic audio to HA while pipeline is active.
    PumpAudioToHa();
}

void Satellite::OnDisconnected() {
    LVA_LOGI(kTag, "HA disconnected — resetting pipeline state");
    StopAudioStreaming();
    StopTimerRing();
    pipeline_active_       = false;
    continue_conversation_ = false;
    continue_listen_at_ns_.store(0, std::memory_order_relaxed);
    Unduck();
    lva::tr::Show(lva::tr::LedState::Idle);
    if (announce_player_) announce_player_->Stop();
    if (state_.music_player) state_.music_player->Stop();
    tts_url_.clear();
    tts_played_ = false;
}

void Satellite::OnMuted() {
    StopAudioStreaming();
    continue_listen_at_ns_.store(0, std::memory_order_relaxed);
    if (pipeline_active_ && announce_player_) {
        announce_player_->Stop();
        pipeline_active_       = false;
        continue_conversation_ = false;
        Unduck();
        lva::tr::Show(lva::tr::LedState::Idle);
    }
}

std::filesystem::path Satellite::ResolveWakeWordConfig(
    const std::string& id) {
    const auto local_path = state_.wakeword_dir / (id + ".json");
    if (std::filesystem::exists(local_path)) {
        return local_path;
    }

    const auto it = external_wake_words_.find(id);
    if (it == external_wake_words_.end()) {
        return {};
    }
    return lva::audio::DownloadExternalWakeWord(it->second,
                                                state_.wakeword_dir);
}

bool Satellite::HandleMessage(std::uint32_t msg_type_id,
                              const ::google::protobuf::MessageLite& msg) {
    using namespace lva::proto;

    switch (msg_type_id) {
        case kIdVoiceAssistantConfigurationRequest: {
            const auto& cfg_req =
                static_cast<const ::VoiceAssistantConfigurationRequest&>(msg);

            ::VoiceAssistantConfigurationResponse resp;
            resp.set_max_active_wake_words(2);

            // Available: scan .json files in wakeword_dir (no TFLite load).
            for (const auto& info :
                 lva::audio::ScanAvailableWakeWords(state_.wakeword_dir)) {
                auto* ww = resp.add_available_wake_words();
                ww->set_id(info.id);
                ww->set_wake_word(info.wake_word);
                for (const auto& lang : info.trained_languages) {
                    ww->add_trained_languages(lang);
                }
            }

            external_wake_words_.clear();
            for (int i = 0; i < cfg_req.external_wake_words_size(); ++i) {
                const auto& eww = cfg_req.external_wake_words(i);
                if (eww.model_type() != "micro") {
                    LVA_LOGD(kTag, "skipping external wake word '%s' "
                                   "(type=%s, not micro)",
                             eww.id().c_str(), eww.model_type().c_str());
                    continue;
                }

                lva::audio::ExternalWakeWordInfo info;
                info.id         = eww.id();
                info.wake_word  = eww.wake_word();
                info.model_type = eww.model_type();
                info.model_size = eww.model_size();
                info.model_hash = eww.model_hash();
                info.url        = eww.url();
                for (int l = 0; l < eww.trained_languages_size(); ++l) {
                    info.trained_languages.push_back(eww.trained_languages(l));
                }

                auto* ww = resp.add_available_wake_words();
                ww->set_id(info.id);
                ww->set_wake_word(info.wake_word);
                for (const auto& lang : info.trained_languages) {
                    ww->add_trained_languages(lang);
                }

                external_wake_words_.emplace(eww.id(), std::move(info));
            }

            // Active: currently loaded in engine (micro + open). Snapshot
            // under the engine's lock so concurrent SetActive* won't
            // invalidate iterators here.
            {
                const auto micro = engine_.SnapshotModels();
                for (const auto& m : micro) {
                    if (m->config().id != "stop")
                        resp.add_active_wake_words(m->config().id);
                }
                const auto open = engine_.SnapshotOpenModels();
                for (const auto& m : open) {
                    resp.add_active_wake_words(m->config().id);
                }
            }

            if (state_.broadcast) {
                state_.broadcast(kIdVoiceAssistantConfigurationResponse,
                                 resp);
            }
            LVA_LOGI(kTag, "ConfigurationRequest: %d available "
                           "(%zu external), %d active",
                     resp.available_wake_words_size(),
                     external_wake_words_.size(),
                     resp.active_wake_words_size());
            return true;
        }

        case kIdVoiceAssistantSetConfiguration: {
            const auto& set_req =
                static_cast<const ::VoiceAssistantSetConfiguration&>(msg);
            std::string ids;
            for (int i = 0; i < set_req.active_wake_words_size(); ++i) {
                if (i > 0) ids += ",";
                ids += set_req.active_wake_words(i);
            }
            LVA_LOGI(kTag, "SetConfiguration: active=[%s]", ids.c_str());

            std::vector<std::shared_ptr<lva::audio::MicroWakeWord>> new_micro;
            std::vector<std::shared_ptr<lva::audio::OpenWakeWord>>  new_open;
            int load_failures = 0;
            for (int i = 0; i < set_req.active_wake_words_size(); ++i) {
                const auto& ww_id = set_req.active_wake_words(i);
                const auto json_path = ResolveWakeWordConfig(ww_id);
                if (json_path.empty()) {
                    ++load_failures;
                    LVA_LOGW(kTag, "SetConfiguration: cannot resolve '%s'",
                             ww_id.c_str());
                    continue;
                }
                const std::string type =
                    lva::audio::ReadWakeWordType(json_path.parent_path(),
                                                 ww_id);

                bool loaded = false;
                if (type == "open") {
                    auto model = lva::audio::OpenWakeWord::FromConfig(json_path);
                    if (model && model->Ok()) {
                        new_open.push_back(std::move(model));
                        loaded = true;
                    }
                } else {
                    // "" or "micro" or anything else → try micro.
                    auto model = lva::audio::MicroWakeWord::FromConfig(json_path);
                    if (model && model->Ok()) {
                        new_micro.push_back(std::move(model));
                        loaded = true;
                    }
                }
                if (!loaded) {
                    ++load_failures;
                    LVA_LOGW(kTag, "SetConfiguration: failed to load '%s'"
                                   " (type=%s)",
                             ww_id.c_str(),
                             type.empty() ? "?" : type.c_str());
                }
            }

            if (!new_micro.empty() || !new_open.empty()) {
                std::size_t slot = 0;
                auto apply_default = [&](float cutoff) {
                    if (slot == 0 &&
                        !state_.preferences.wake_word_1_sensitivity.has_value()) {
                        state_.preferences.wake_word_1_sensitivity = cutoff;
                    } else if (slot == 1 &&
                        !state_.preferences.wake_word_2_sensitivity.has_value()) {
                        state_.preferences.wake_word_2_sensitivity = cutoff;
                    }
                    ++slot;
                };
                for (const auto& m : new_micro) {
                    if (slot >= 2) break;
                    apply_default(m->config().probability_cutoff);
                }
                for (const auto& m : new_open) {
                    if (slot >= 2) break;
                    apply_default(m->config().probability_cutoff);
                }

                // Persist the selection so it survives a cold reboot.
                // Store the ids actually loaded, in slot order (micro
                // first, then open), matching how GetConfiguration
                // reports the active wake words. Must run before the
                // std::move below empties the vectors.
                std::vector<std::optional<std::string>> selected;
                selected.reserve(new_micro.size() + new_open.size());
                for (const auto& m : new_micro) {
                    selected.emplace_back(m->config().id);
                }
                for (const auto& m : new_open) {
                    selected.emplace_back(m->config().id);
                }
                state_.preferences.active_wake_words = std::move(selected);

                engine_.SetActiveModels(std::move(new_micro));
                engine_.SetActiveOpenModels(std::move(new_open));
                state_.SavePreferences();
                // Broadcast updated entity states to HA.
                if (state_.broadcast) {
                    const auto sink = [&](std::uint32_t id,
                                          const ::google::protobuf::MessageLite& m) {
                        state_.broadcast(id, m);
                    };
                    for (const auto& entity : state_.entities) {
                        entity->OnSubscribeStates(sink);
                    }
                }
            } else {
                LVA_LOGW(kTag, "SetConfiguration: no valid models "
                               "(%d failures); keeping current set",
                         load_failures);
            }
            return true;
        }

        case kIdVoiceAssistantEventResponse: {
            const auto& ev =
                static_cast<const ::VoiceAssistantEventResponse&>(msg);
            std::vector<std::pair<std::string, std::string>> data;
            data.reserve(ev.data_size());
            for (int i = 0; i < ev.data_size(); ++i) {
                const auto& kv = ev.data(i);
                data.emplace_back(kv.name(), kv.value());
            }
            OnVoiceEvent(static_cast<int>(ev.event_type()), data);
            return true;
        }

        case kIdVoiceAssistantTimerEventResponse: {
            const auto& timer_msg =
                static_cast<const ::VoiceAssistantTimerEventResponse&>(msg);
            const int evt = static_cast<int>(timer_msg.event_type());
            if (evt == 3) {
                LVA_LOGI(kTag, "TIMER_FINISHED — ringing");
                StartTimerRing();
            } else if (evt == 2) {
                LVA_LOGI(kTag, "TIMER_CANCELLED — stopping ring");
                StopTimerRing();
            }
            return true;
        }

        case kIdVoiceAssistantAnnounceRequest: {
            const auto& ann =
                static_cast<const ::VoiceAssistantAnnounceRequest&>(msg);
            LVA_LOGI(kTag, "AnnounceRequest: media_id=%s preannounce=%s",
                     ann.media_id().c_str(),
                     ann.preannounce_media_id().c_str());
            PlayAnnounce(ann.media_id(), ann.preannounce_media_id(),
                         ann.start_conversation());
            return true;
        }

        default:
            return false;
    }
}

}  // namespace lva::satellite
