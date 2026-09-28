#include "VoiceAssistantNode.h"

#include <getopt.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>

#include "audio/AudioCapture.h"
#include "audio/LibMpvPlayer.h"
#include "audio/MicroWakeWord.h"
#include "audio/OpenWakeWord.h"
#include "audio/OpenWakeWordFeatures.h"
#include "audio/PcmRingBuffer.h"
#include "audio/WakeWordEngine.h"
#include "audio/WebRtcProcessor.h"
#include "entities/EventEntity.h"
#include "entities/MediaPlayerEntity.h"
#include "entities/MuteSwitchEntity.h"
#include "entities/NumberEntity.h"
#include "entities/SelectEntity.h"
#include "entities/ThinkingSoundEntity.h"
#include "entities/UpdateEntity.h"
#include "protocol/ApiServer.h"
#include "protocol/MdnsPublisher.h"
#include "satellite/Satellite.h"
#include "state/Preferences.h"
#include "state/ServerState.h"
#include "tr/HomeButton.h"
#include "tr/MicMuteGpio.h"
#include "tr/SoundConfWatcher.h"
#include "tr/Supervisor.h"
#include "tr/SupervisorHttpServer.h"
#include "tr/SysInfo.h"
#include "tr/SystemVolume.h"
#include "util/Log.h"

namespace {
constexpr const char* kTag = "VoiceAssistantNode";
constexpr const char* kVersion = "0.0.1";
constexpr double kDefaultMicroWakeSensitivity = 0.85;
constexpr double kDefaultOpenWakeSensitivity  = 0.5;
constexpr double kDefaultStopSensitivity      = 0.5;


std::atomic<int> g_shutdown_signal{0};
lva::proto::ApiServer* g_server = nullptr;
lva::audio::WakeWordEngine* g_wakeword_engine = nullptr;



extern "C" void OnSignal(int signo) {
    g_shutdown_signal.store(signo, std::memory_order_relaxed);
    if (g_wakeword_engine) {
        g_wakeword_engine->RequestStop();
    }
    if (g_server) {
        g_server->Stop();
    }
}

extern "C" void OnSigchld(int /*signo*/) {
    int saved_errno = errno;
    while (::waitpid(-1, nullptr, WNOHANG) > 0) {
    }
    errno = saved_errno;
}

void InstallSignalHandlers() {
    struct sigaction sa{};
    sa.sa_handler = &OnSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ::sigaction(SIGTERM, &sa, nullptr);
    ::sigaction(SIGINT, &sa, nullptr);
    std::signal(SIGPIPE, SIG_IGN);

    struct sigaction sc{};
    sc.sa_handler = &OnSigchld;
    sigemptyset(&sc.sa_mask);
    sc.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    ::sigaction(SIGCHLD, &sc, nullptr);
}


std::string ReadMacFromDeviceJson(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return {};
    std::ifstream f(path);
    if (!f) return {};
    nlohmann::json j;
    try {
        f >> j;
    } catch (...) {
        return {};
    }
    if (!j.is_object()) return {};
    if (auto it = j.find("device"); it != j.end() && it->is_object()) {
        if (auto mit = it->find("macAddress"); mit != it->end() && mit->is_string()) {
            return mit->get<std::string>();
        }
    }
    return {};
}
}  // namespace

VoiceAssistantNode::VoiceAssistantNode(VoiceAssistantConfig config)
    : config(std::move(config)) {}

VoiceAssistantNode::~VoiceAssistantNode() {
    Stop();
}

bool VoiceAssistantNode::Start() {
    if (running_.load()) return true;
    setlocale(LC_NUMERIC, "C");
    lva::esphome_log::SetLevel(cli.debug ? lva::esphome_log::Level::kDebug
                                 : lva::esphome_log::Level::kInfo);

    LVA_LOGI(kTag,
             "linux-voice-assistant-cpp %s starting "
             "(name=%s host=%s port=%u prefs=%s)",
             kVersion, cli.device_name.c_str(), cli.host.c_str(),
             static_cast<unsigned>(cli.port),
             cli.preferences_file.c_str());

    // Build shared ServerState. Owned by main; ApiServer holds a ref.
    lva::state::ServerState state;
    state.name             = cli.device_name;
    state.friendly_name    = cli.device_name;
    {
        const lva::tr::DeviceInfo dev = lva::tr::ReadDeviceInfo();
        if (!dev.firmware_version.empty()) {
            state.version = dev.firmware_version;
        } else {
            LVA_LOGW(kTag, "device.firmwareVersion missing; "
                           "falling back to package version %s",
                     kVersion);
            state.version = kVersion;
        }
        if (!dev.mac_address.empty()) {
            state.mac_address = dev.mac_address;
        }
    }
    state.esphome_version  = "2025.9.0";
    if (state.mac_address.empty()) {
        state.mac_address = ReadMacFromDeviceJson("/data/conf/device.json");
    }
    std::transform(state.mac_address.begin(), state.mac_address.end(),
                   state.mac_address.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    state.preferences_path = cli.preferences_file;
    const bool use_openwakeword = (cli.wakeword_type == "open");
    const std::filesystem::path wakeword_dir = use_openwakeword
        ? "/usr/share/thirdreality/wakewords/openwakeword"
        : "/usr/share/thirdreality/wakewords/microwakeword";
    state.wakeword_dir = wakeword_dir;
    state.preferences      = lva::state::Preferences::LoadFromFile(
        cli.preferences_file);

    if (state.preferences.volume.has_value()) {
        state.volume.store(*state.preferences.volume,
                           std::memory_order_relaxed);
        lva::tr::SetSystemVolumeSilent(
            static_cast<int>(*state.preferences.volume * 100.0 + 0.5));
    }
    state.muted.store(state.preferences.is_mic_muted(),
                      std::memory_order_relaxed);
    state.mic_volume_live.store(state.preferences.mic_volume,
                                std::memory_order_relaxed);
    // Prefer the persisted delay; fall back to the CLI default when it
    // was never set from HA.
    {
        const double delay_s =
            state.preferences.continue_conversation_delay.value_or(
                cli.continue_conversation_delay);
        state.continue_conversation_delay_ns =
            static_cast<std::int64_t>(delay_s * 1e9);
    }

    auto mic_mute_gpio = std::make_unique<lva::tr::MicMuteGpio>(state);
    state.mic_mute_gpio = mic_mute_gpio.get();
    if (mic_mute_gpio->Available()) {
        mic_mute_gpio->ReadAndApplyOnce();
    }

    lva::audio::LibMpvPlayer::Options mpv_opts;
    mpv_opts.audio_device = cli.audio_device;
    mpv_opts.cache_secs   = 10;
    mpv_opts.short_sound_safe = false;
    auto music_player = std::make_unique<lva::audio::LibMpvPlayer>(mpv_opts);

    lva::audio::LibMpvPlayer::Options tts_opts;
    tts_opts.audio_device     = cli.audio_device;
    tts_opts.cache_secs       = 1;     // smaller for low TTS latency
    tts_opts.short_sound_safe = true;
    auto announce_player = std::make_unique<lva::audio::LibMpvPlayer>(tts_opts);

    auto pcm_ring = std::make_unique<lva::audio::PcmRingBuffer>(
        16'000 * 2);
    auto satellite_ring = std::make_unique<lva::audio::PcmRingBuffer>(
        16'000 * 2);

    // Parse capture backend + ref channels.
    lva::audio::AudioCapture::Options cap_opts;
    const bool use_alsa_backend = (cli.capture_backend == "alsa");
    if (use_alsa_backend) {
        cap_opts.backend            = lva::audio::AudioCapture::Backend::kAlsa;
        cap_opts.alsa_device        = cli.capture_alsa_device;
        cap_opts.alsa_channels      = 4;
        cap_opts.mic_channel        =
            static_cast<unsigned>(std::max(0, cli.capture_mic_channel));
        cap_opts.frames_per_read    = 160;   // 10 ms; matches WebRTC frame
        // Parse "a,b" / "a" / "" / "none" into ref_channels[2].
        std::array<int, 2> refs = {-1, -1};
        if (!cli.capture_ref_channels.empty() &&
            cli.capture_ref_channels != "none") {
            std::size_t comma = cli.capture_ref_channels.find(',');
            try {
                refs[0] = std::stoi(cli.capture_ref_channels.substr(0, comma));
                if (comma != std::string::npos) {
                    refs[1] = std::stoi(cli.capture_ref_channels.substr(comma + 1));
                }
            } catch (...) {
                LVA_LOGW(kTag,
                         "could not parse --capture-ref-channels '%s'; "
                         "AEC disabled",
                         cli.capture_ref_channels.c_str());
                refs = {-1, -1};
            }
        }
        cap_opts.ref_channels = refs;
    } else if (cli.capture_backend == "pulse") {
        cap_opts.backend            = lva::audio::AudioCapture::Backend::kPulse;
        cap_opts.frames_per_read    = 1600;   // 100 ms (legacy default)
        cap_opts.buffer_latency_us  = 100'000;
    } else {
        LVA_LOGE(kTag, "unknown --capture-backend '%s'; expected alsa|pulse",
                 cli.capture_backend.c_str());
        return 2;
    }

    auto audio_capture = std::make_unique<lva::audio::AudioCapture>(
        cap_opts, *pcm_ring);
    audio_capture->AddTap(*satellite_ring);

    // Tracks whether we materialized any first-boot default into
    // preferences; if so we persist once below so /data/conf/sound.json
    // becomes a complete snapshot of every setting on first run.
    bool prefs_dirty = false;
    if (state.preferences.mic_auto_gain == 0) {
        state.preferences.mic_auto_gain = 10;
        prefs_dirty = true;
    }
    if (state.preferences.mic_noise_suppression == 0) {
        state.preferences.mic_noise_suppression = 2;  // Medium NS
        prefs_dirty = true;
    }
    if (state.preferences.mic_volume == 100) {
        state.preferences.mic_volume = 1600;
        state.mic_volume_live.store(1600, std::memory_order_relaxed);
        prefs_dirty = true;
    }
    if (prefs_dirty) {
        LVA_LOGI(kTag,
                 "applied first-boot mic defaults: agc=%d, ns=%d, vol=%d",
                 state.preferences.mic_auto_gain,
                 state.preferences.mic_noise_suppression,
                 state.preferences.mic_volume);
    }

    const bool aec_enabled = use_alsa_backend &&
                             (cap_opts.ref_channels[0] >= 0);
    LVA_LOGI(kTag, "capture backend=%s aec=%s",
             use_alsa_backend ? "alsa" : "pulse",
             aec_enabled ? "enabled" : "disabled");

    auto webrtc_processor = std::make_unique<lva::audio::WebRtcProcessor>(
        state.preferences.mic_auto_gain,
        state.preferences.mic_noise_suppression,
        aec_enabled);
    audio_capture->SetProcessor(webrtc_processor.get());
    audio_capture->SetMicVolumeSource(&state.mic_volume_live);
    state.audio_capture = audio_capture.get();
    state.webrtc_processor = webrtc_processor.get();
    state.announce_player  = announce_player.get();
    state.music_player     = music_player.get();

    // Set initial volume on announce player to match music player.
    if (state.preferences.volume.has_value()) {
        announce_player->SetVolume(*state.preferences.volume);
    }

    std::uint32_t next_key = 0;

    {
        auto media = std::make_unique<lva::entities::MediaPlayerEntity>(
            next_key++, state, music_player.get());
        state.media_player_entity = media.get();
        state.entities.push_back(std::move(media));
    }

    {
        auto mute = std::make_unique<lva::entities::MuteSwitchEntity>(
            next_key++, state);
        state.mute_switch_entity = mute.get();
        state.entities.push_back(std::move(mute));
    }
    state.entities.push_back(std::make_unique<lva::entities::ThinkingSoundEntity>(
        next_key++, state));


    state.entities.push_back(std::make_unique<lva::entities::NumberEntity>(
        next_key++,
        lva::entities::NumberEntity::Config{
            .object_id     = "wake_word_1_sensitivity",
            .display_name  = "Wake Word 1 Sensitivity",
            .icon          = "",
            .min_value     = 0.0,
            .max_value     = 1.0,
            .step          = 0.001,
            .mode_enum     = 1,  // BOX
            .getter        = [&state, use_openwakeword] {
                const double fallback = use_openwakeword
                    ? kDefaultOpenWakeSensitivity
                    : kDefaultMicroWakeSensitivity;
                return state.preferences.wake_word_1_sensitivity.value_or(
                    fallback);
            },
            .setter        = [&state](double v) {
                state.PersistWakeWordSensitivity(1, v);
            },
        }));

    state.entities.push_back(std::make_unique<lva::entities::NumberEntity>(
        next_key++,
        lva::entities::NumberEntity::Config{
            .object_id     = "wake_word_2_sensitivity",
            .display_name  = "Wake Word 2 Sensitivity",
            .icon          = "",
            .min_value     = 0.0,
            .max_value     = 1.0,
            .step          = 0.001,
            .mode_enum     = 1,
            .getter        = [&state, use_openwakeword] {
                const double fallback = use_openwakeword
                    ? kDefaultOpenWakeSensitivity
                    : kDefaultMicroWakeSensitivity;
                return state.preferences.wake_word_2_sensitivity.value_or(
                    fallback);
            },
            .setter        = [&state](double v) {
                state.PersistWakeWordSensitivity(2, v);
            },
        }));

    state.entities.push_back(std::make_unique<lva::entities::NumberEntity>(
        next_key++,
        lva::entities::NumberEntity::Config{
            .object_id     = "stop_word_sensitivity",
            .display_name  = "Stop Word Sensitivity",
            .icon          = "mdi:hand-back-left",
            .min_value     = 0.0,
            .max_value     = 1.0,
            .step          = 0.001,
            .mode_enum     = 1,
            .getter        = [&state] {
                return state.preferences.stop_word_sensitivity.value_or(
                    kDefaultStopSensitivity);
            },
            .setter        = [&state](double v) {
                state.PersistStopWordSensitivity(v);
            },
        }));

    // Continued-conversation listen delay (seconds). Persisted so it
    // survives a cold reboot; getter falls back to the CLI-derived
    // default when never set.
    state.entities.push_back(std::make_unique<lva::entities::NumberEntity>(
        next_key++,
        lva::entities::NumberEntity::Config{
            .object_id     = "continue_conversation_delay",
            .display_name  = "Continue Conversation Delay",
            .icon          = "mdi:timer-sand",
            .min_value     = 0.0,
            .max_value     = 10.0,
            .step          = 0.1,
            .mode_enum     = 1,  // BOX
            .getter        = [&state, default_ccd = cli.continue_conversation_delay] {
                return state.preferences.continue_conversation_delay.value_or(
                    default_ccd);
            },
            .setter        = [&state](double v) {
                state.PersistContinueConversationDelay(v);
            },
        }));

    state.entities.push_back(std::make_unique<lva::entities::NumberEntity>(
        next_key++,
        lva::entities::NumberEntity::Config{
            .object_id     = "mic_gain",
            .display_name  = "Mic Auto Gain",
            .icon          = "mdi:microphone-plus",
            .min_value     = 0.0,
            .max_value     = 31.0,
            .step          = 1.0,
            .mode_enum     = 0,  // AUTO
            .getter        = [&state] {
                return static_cast<double>(state.preferences.mic_auto_gain);
            },
            .setter        = [&state](double v) {
                state.PersistMicAutoGain(static_cast<int>(v));
            },
        }));

    // Mic noise suppression: 5-level select. Maps label ↔ int 0..4.
    {
        static const std::vector<std::string> kNoiseOptions = {
            "Off", "Low", "Medium", "High", "Max",
        };
        state.entities.push_back(std::make_unique<lva::entities::SelectEntity>(
            next_key++,
            lva::entities::SelectEntity::Config{
                .object_id     = "mic_noise",
                .display_name  = "Mic Noise Suppression",
                .icon          = "mdi:waveform",
                .options       = kNoiseOptions,
                .getter        = [&state]() -> std::string {
                    const int idx = std::clamp(
                        state.preferences.mic_noise_suppression, 0, 4);
                    return kNoiseOptions[static_cast<std::size_t>(idx)];
                },
                .setter        = [&state](const std::string& label) {
                    for (std::size_t i = 0; i < kNoiseOptions.size(); ++i) {
                        if (kNoiseOptions[i] == label) {
                            state.PersistMicNoiseSuppression(
                                static_cast<int>(i));
                            return;
                        }
                    }
                    LVA_LOGW(kTag, "mic_noise: unknown label '%s'",
                             label.c_str());
                },
            }));
    }

    state.entities.push_back(std::make_unique<lva::entities::NumberEntity>(
        next_key++,
        lva::entities::NumberEntity::Config{
            .object_id     = "mic_volume",
            .display_name  = "Mic Volume",
            .icon          = "mdi:microphone-settings",
            .min_value     = 1.0,
            .max_value     = 4000.0,
            .step          = 1.0,
            .mode_enum     = 0,  // AUTO
            .getter        = [&state] {
                return static_cast<double>(state.preferences.mic_volume);
            },
            .setter        = [&state](double v) {
                state.PersistMicVolume(static_cast<int>(v));
            },
        }));

    const std::uint32_t home_button_entity_key = next_key++;
    state.entities.push_back(std::make_unique<lva::entities::EventEntity>(
        home_button_entity_key,
        lva::entities::EventEntity::Config{
            .object_id    = "thirdreality_home_button",
            .display_name = "Home Button",
            .icon         = "mdi:gesture-tap-button",
            .event_types  = {"single_press", "double_press", "triple_press"},
        }));

    lva::entities::UpdateEntity* update_entity_ptr = nullptr;
    {
        auto upd = std::make_unique<lva::entities::UpdateEntity>(
            next_key++, state);
        update_entity_ptr = upd.get();
        state.entities.push_back(std::move(upd));
    }

    LVA_LOGI(kTag, "registered %zu entities", state.entities.size());

    if (update_entity_ptr != nullptr) {
        state.on_client_authenticated = [update_entity_ptr] {
            update_entity_ptr->TriggerCheck();
        };
    }

    lva::proto::ApiServer::Options opts;
    opts.bind_address = cli.host;
    opts.port         = cli.port;

    lva::proto::ApiServer server(state, opts);
    g_server = &server;
    InstallSignalHandlers();

    if (!server.Start()) {
        LVA_LOGE(kTag, "ApiServer::Start failed");
        g_server = nullptr;
        return 1;
    }

    // mDNS: register _esphomelib._tcp so HA auto-discovers us.
    lva::proto::MdnsPublisher mdns;
    {
        lva::proto::MdnsPublisher::Options mdns_opts;
        mdns_opts.name    = state.name;
        mdns_opts.port    = cli.port;
        mdns_opts.mac     = state.mac_address;
        mdns_opts.version = state.esphome_version;
        if (!mdns.Start(mdns_opts)) {
            LVA_LOGW(kTag, "mDNS registration failed; device won't "
                           "be auto-discovered by HA");
        }
    }

    if (music_player->WakeupFd() >= 0) {
        server.AddAuxFd(music_player->WakeupFd(),
                        [&music_player] {
                            music_player->DrainEvents();
                        });
    }
    if (announce_player->WakeupFd() >= 0) {
        server.AddAuxFd(announce_player->WakeupFd(),
                        [&announce_player] {
                            announce_player->DrainEvents();
                        });
    }

    if (!audio_capture->Start()) {
        LVA_LOGW(kTag, "audio capture failed to start; mic-using "
                       "features (wake word) will be unavailable");
    }

    auto wakeword_engine =
        std::make_unique<lva::audio::WakeWordEngine>(*pcm_ring);

    {
        // Prefer the persisted selection (survives cold reboot); fall
        // back to the CLI default only when nothing was ever saved.
        std::vector<std::string> ids;
        for (const auto& slot : state.preferences.active_wake_words) {
            if (slot.has_value() && !slot->empty()) ids.push_back(*slot);
        }
        if (ids.empty()) {
            const std::string& list = cli.wakeword_models;
            std::size_t start = 0;
            while (start < list.size()) {
                std::size_t comma = list.find(',', start);
                if (comma == std::string::npos) comma = list.size();
                std::string id(list, start, comma - start);
                while (!id.empty() && std::isspace(static_cast<unsigned char>(id.front()))) id.erase(0,1);
                while (!id.empty() && std::isspace(static_cast<unsigned char>(id.back())))  id.pop_back();
                if (!id.empty()) ids.push_back(std::move(id));
                start = comma + 1;
            }
            LVA_LOGI(kTag, "no persisted wake words; using CLI default '%s'",
                     cli.wakeword_models.c_str());
        } else {
            LVA_LOGI(kTag, "loading %zu persisted wake word(s)", ids.size());
        }

        for (const auto& id : ids) {
            const auto json_path = wakeword_dir / (id + ".json");
            if (use_openwakeword) {
                auto model = lva::audio::OpenWakeWord::FromConfig(json_path);
                if (model && model->Ok()) {
                    wakeword_engine->AddOpenModel(std::move(model));
                } else {
                    LVA_LOGW(kTag, "failed to load OWW '%s'", id.c_str());
                }
            } else {
                auto model = lva::audio::MicroWakeWord::FromConfig(json_path);
                if (model && model->Ok()) {
                    wakeword_engine->AddModel(std::move(model));
                } else {
                    LVA_LOGW(kTag, "failed to load wake word '%s'", id.c_str());
                }
            }
        }
    }

    if (use_openwakeword) {
        auto oww_feat = std::make_unique<lva::audio::OpenWakeWordFeatures>();
        if (oww_feat->Load(wakeword_dir)) {
            wakeword_engine->SetOpenFeatures(std::move(oww_feat));
        } else {
            LVA_LOGE(kTag, "OpenWakeWord features failed to load");
        }
    }

    if (!use_openwakeword) {
        // Stop word only for micro mode (micro-specific model)
        const std::filesystem::path micro_dir =
            "/usr/share/thirdreality/wakewords/microwakeword";
        const auto stop_json = micro_dir / "stop.json";
        auto stop_model = lva::audio::MicroWakeWord::FromConfig(stop_json);
        if (stop_model && stop_model->Ok()) {
            wakeword_engine->AddModel(std::move(stop_model));
        } else {
            LVA_LOGW(kTag, "stop word model not loaded");
        }
    }

    // Apply persisted sensitivities to the freshly loaded models. The
    // model files carry a built-in default cutoff; without this the
    // user's saved wake/stop sensitivities are silently ignored after a
    // (cold) reboot even though HA shows the stored slider value. Slot
    // ordering mirrors PersistWakeWordSensitivity(): non-stop micro
    // models first, then open models; the stop model is separate.
    {
        std::size_t wake_idx = 0;
        const auto micro_models = wakeword_engine->SnapshotModels();
        for (const auto& m : micro_models) {
            if (m->config().id == "stop") {
                if (state.preferences.stop_word_sensitivity.has_value()) {
                    m->SetProbabilityCutoff(static_cast<float>(
                        *state.preferences.stop_word_sensitivity));
                }
                continue;
            }
            const auto& slot = (wake_idx == 0)
                ? state.preferences.wake_word_1_sensitivity
                : state.preferences.wake_word_2_sensitivity;
            if (wake_idx < 2 && slot.has_value()) {
                m->SetProbabilityCutoff(static_cast<float>(*slot));
            }
            ++wake_idx;
        }
        const auto open_models = wakeword_engine->SnapshotOpenModels();
        for (const auto& m : open_models) {
            const auto& slot = (wake_idx == 0)
                ? state.preferences.wake_word_1_sensitivity
                : state.preferences.wake_word_2_sensitivity;
            if (wake_idx < 2 && slot.has_value()) {
                m->SetProbabilityCutoff(static_cast<float>(*slot));
            }
            ++wake_idx;
        }
    }

    // Materialize first-boot defaults for the optional settings so that
    // /data/conf/sound.json ends up holding an explicit value for every
    // configuration item, not just the ones the user has touched. Only
    // fill slots that have a real wake-word model loaded; the stop-word
    // default is only meaningful in micro mode (where a stop model
    // exists). Anything already set (loaded from disk) is left as-is.
    {
        const double wake_default = use_openwakeword
            ? kDefaultOpenWakeSensitivity
            : kDefaultMicroWakeSensitivity;
        std::size_t loaded_wake_words =
            wakeword_engine->SnapshotOpenModels().size();
        for (const auto& m : wakeword_engine->SnapshotModels()) {
            if (m->config().id != "stop") ++loaded_wake_words;
        }

        if (loaded_wake_words >= 1 &&
            !state.preferences.wake_word_1_sensitivity.has_value()) {
            state.preferences.wake_word_1_sensitivity = wake_default;
            prefs_dirty = true;
        }
        if (loaded_wake_words >= 2 &&
            !state.preferences.wake_word_2_sensitivity.has_value()) {
            state.preferences.wake_word_2_sensitivity = wake_default;
            prefs_dirty = true;
        }
        if (!use_openwakeword &&
            !state.preferences.stop_word_sensitivity.has_value()) {
            state.preferences.stop_word_sensitivity = kDefaultStopSensitivity;
            prefs_dirty = true;
        }
        if (!state.preferences.continue_conversation_delay.has_value()) {
            state.preferences.continue_conversation_delay =
                cli.continue_conversation_delay;
            prefs_dirty = true;
        }
    }

    // Persist once if any first-boot default was materialized above, so
    // the on-disk config is complete after the very first run.
    if (prefs_dirty) {
        if (state.SavePreferences()) {
            LVA_LOGI(kTag, "persisted complete config to %s",
                     state.preferences_path.c_str());
        } else {
            LVA_LOGW(kTag, "failed to persist first-boot config to %s",
                     state.preferences_path.c_str());
        }
    }

    // Voice satellite state machine.
    auto satellite = std::make_unique<lva::satellite::Satellite>(
        state, *satellite_ring, *wakeword_engine, announce_player.get());
    state.satellite = satellite.get();

    // When HA disconnects, reset satellite state.
    state.on_client_disconnected = [&satellite] {
        satellite->OnDisconnected();
    };

    auto sound_watcher = std::make_unique<lva::tr::SoundConfWatcher>(
        state, cli.preferences_file);

    auto supervisor = std::make_unique<lva::tr::Supervisor>();
    state.supervisor = supervisor.get();
    auto supervisor_http =
        std::make_unique<lva::tr::SupervisorHttpServer>(*supervisor);
    if (!supervisor_http->Start()) {
        LVA_LOGW(kTag, "supervisor HTTP failed to start; OTA over "
                       "the legacy supervisor API will not be "
                       "available (HA UI Update entity still works)");
    }

    wakeword_engine->SetDetectionCallback(
        [&satellite](const std::string& model_id, float prob) {
            if (model_id == "stop") {
                LVA_LOGD("wake",
                         "stop word detected prob=%.3f", prob);
                satellite->OnStopDetected();
            } else {
                LVA_LOGD("wake",
                         "wake word detected id=%s prob=%.3f",
                         model_id.c_str(), prob);
                satellite->OnWakeDetected(model_id, prob);
            }
        });

    state.wakeword_engine = wakeword_engine.get();
    g_wakeword_engine = wakeword_engine.get();

    if (!wakeword_engine->Start()) {
        LVA_LOGW(kTag, "wake-word engine failed to start; voice "
                       "pipeline disabled");
    }

    const int satellite_tick_fd = ::timerfd_create(CLOCK_MONOTONIC,
                                                   TFD_CLOEXEC | TFD_NONBLOCK);
    if (satellite_tick_fd >= 0) {
        itimerspec ts{};
        ts.it_value.tv_sec     = 0;
        ts.it_value.tv_nsec    = 50'000'000;  // first fire 50 ms
        ts.it_interval.tv_sec  = 0;
        ts.it_interval.tv_nsec = 50'000'000;  // every 50 ms
        if (::timerfd_settime(satellite_tick_fd, 0, &ts, nullptr) == 0) {
            server.AddAuxFd(satellite_tick_fd,
                            [&satellite, &mic_mute_gpio, &sound_watcher,
                             update_entity_ptr, satellite_tick_fd] {
                std::uint64_t expirations = 0;
                while (::read(satellite_tick_fd,
                              &expirations, sizeof(expirations)) > 0) {
                    // drain — we don't care about the count
                }
                satellite->OnLoopTick();
                mic_mute_gpio->Poll();
                sound_watcher->Poll();
                if (update_entity_ptr) update_entity_ptr->OnPeriodicTick();
            });
        } else {
            LVA_LOGW(kTag, "timerfd_settime failed; satellite ticks disabled");
            ::close(satellite_tick_fd);
        }
    } else {
        LVA_LOGW(kTag, "timerfd_create failed; satellite ticks disabled");
    }

    lva::tr::HomeButton::Options home_btn_opts;
    home_btn_opts.entity_key = home_button_entity_key;
    auto home_button = std::make_unique<lva::tr::HomeButton>(
        home_btn_opts, state);
    {
        const int hb_fd = home_button->Start();
        if (hb_fd >= 0) {
            server.AddAuxFd(hb_fd, [&home_button] {
                home_button->OnMainLoopWake();
            });
        } else {
            LVA_LOGW(kTag, "home button monitor disabled");
        }
    }


    const int rc = server.Run();
    running.store(true);
    return true;
}

void VoiceAssistantNode::Stop() {
    if (!running_.load()) return;
    LVA_LOGI(kTag, "Stopping VoiceAssistantNode...");

    if (wakeword_engine_) wakeword_engine_->Stop();
    if (audio_capture_) audio_capture_->Stop();
    if (server_) server_->Stop();

    running.store(false);
}