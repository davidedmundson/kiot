#include "sendspinclient.h"
#include <QString>
#include <QHostInfo>
#include <QSysInfo>
#include <KIOTShared/kiotshared.h>
DEFINE_PLUGIN_LOGGER(spclient, SendSpinPlugin) //Change TeplatePlugin to you plugin name for better logs

// Alle dine eksisterende includes fra sendspin-client.cpp:
#include <sendspin/client.h>
#include <sendspin/color_role.h>
#include <sendspin/controller_role.h>
#include <sendspin/metadata_role.h>
#include <sendspin/player_role.h>
#include <sendspin/visualizer_role.h>
#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <linux/input.h>
#include <mutex>
#include <poll.h>
#include <unistd.h>

using namespace sendspin;

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_ducked{false};
static std::atomic<int> g_tap_action{0};  // 0=none, 1=single tap, 2=double tap
static void signal_handler(int) { g_running = false; }
static void sigusr1_handler(int) { g_ducked = true; }
static void sigusr2_handler(int) { g_ducked = false; }


static constexpr const char *SOUND_CONF = "/data/conf/sound.json";
static constexpr const char *SENDSPIN_CONF = "/data/conf/sendspin.json";

// ============================================================================
// JSON helpers
// ============================================================================

static int read_json_int(const char *path, const char *field) {
  std::ifstream f(path);
  if (!f) return -1;
  std::string content((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
  std::string key = std::string("\"") + field + "\"";
  auto pos = content.find(key);
  if (pos == std::string::npos) return -1;
  pos = content.find(':', pos);
  if (pos == std::string::npos) return -1;
  return std::atoi(content.c_str() + pos + 1);
}

static void write_json_int(const char *path, const char *field, int value) {
  std::ifstream in(path);
  if (!in) return;
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  in.close();

  std::string key = std::string("\"") + field + "\"";
  auto pos = content.find(key);
  if (pos == std::string::npos) return;
  pos = content.find(':', pos);
  if (pos == std::string::npos) return;
  pos++;
  while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) pos++;
  auto end = pos;
  if (end < content.size() && content[end] == '-') end++;
  while (end < content.size() && (content[end] >= '0' && content[end] <= '9')) end++;
  if (end == pos) return;

  char buf[16];
  snprintf(buf, sizeof(buf), "%d", value);
  content.replace(pos, end - pos, buf);

  std::string tmp = std::string(path) + ".tmp";
  std::ofstream out(tmp, std::ios::trunc);
  if (!out) return;
  out << content;
  out.close();
  rename(tmp.c_str(), path);
}

static int read_device_volume() {
  return read_json_int(SOUND_CONF, "volume");
}

static void persist_volume(int vol) {
  if (vol < 0) vol = 0;
  if (vol > 100) vol = 100;
  write_json_int(SOUND_CONF, "volume", vol);
}

static int64_t now_us() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ============================================================================
// Persistence Provider — saves state to /data/conf/sendspin.json
// ============================================================================

class FilePersistenceProvider : public SendspinPersistenceProvider {
 public:
  FilePersistenceProvider() {
    // Ensure all required fields exist in config file
    std::ifstream test(SENDSPIN_CONF);
    if (!test) {
      std::ofstream out(SENDSPIN_CONF, std::ios::trunc);
      out << "{\n  \"last_server_hash\": 0,\n  \"static_delay_ms\": 0,\n  \"led_disabled\": 1\n}\n";
    } else {
      // Check if led_disabled field exists, add it if missing
      std::string content((std::istreambuf_iterator<char>(test)),
                          std::istreambuf_iterator<char>());
      test.close();
      if (content.find("\"led_disabled\"") == std::string::npos) {
        auto pos = content.rfind('}');
        if (pos != std::string::npos) {
          content.insert(pos, ",\n  \"led_disabled\": 1\n");
          std::ofstream out(SENDSPIN_CONF, std::ios::trunc);
          out << content;
        }
      }
    }
  }

  bool save_last_server_hash(uint32_t hash) override {
    write_json_int(SENDSPIN_CONF, "last_server_hash", static_cast<int>(hash));

    qCInfo(spclient) << "persisted last_server_hash: " << hash;
    return true;
  }

  std::optional<uint32_t> load_last_server_hash() override {
    int val = read_json_int(SENDSPIN_CONF, "last_server_hash");
    if (val < 0) return std::nullopt;
    return static_cast<uint32_t>(val);
  }

  bool save_static_delay(uint16_t delay_ms) override {
    write_json_int(SENDSPIN_CONF, "static_delay_ms", delay_ms);
    qCInfo(spclient) << "persisted static_delay:" <<  delay_ms;
    return true;
  }

  std::optional<uint16_t> load_static_delay() override {
    int val = read_json_int(SENDSPIN_CONF, "static_delay_ms");
    if (val < 0) return std::nullopt;
    return static_cast<uint16_t>(val);
  }
};

// ============================================================================
// PulseAudio volume/mute controller (async, non-blocking)
// ============================================================================

class PulseVolumeController {
 public:
  PulseVolumeController() {
    ml_ = pa_threaded_mainloop_new();
    if (!ml_) return;
    ctx_ = pa_context_new(pa_threaded_mainloop_get_api(ml_), "sendspin-vol");
    if (!ctx_) return;
    pa_context_set_state_callback(ctx_, [](pa_context*, void*){}, nullptr);
    pa_context_connect(ctx_, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
    pa_threaded_mainloop_start(ml_);
  }

  ~PulseVolumeController() {
    if (ml_) {
      pa_threaded_mainloop_stop(ml_);
      if (ctx_) { pa_context_disconnect(ctx_); pa_context_unref(ctx_); }
      pa_threaded_mainloop_free(ml_);
    }
  }

  void set_volume_percent(int percent) {
    if (!ctx_ || !ml_) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    pa_threaded_mainloop_lock(ml_);
    if (pa_context_get_state(ctx_) == PA_CONTEXT_READY) {
      pa_cvolume vol;
      pa_cvolume_set(&vol, 2, pa_sw_volume_from_linear(percent / 100.0));
      auto *op = pa_context_set_sink_volume_by_name(ctx_, "@DEFAULT_SINK@", &vol, nullptr, nullptr);
      if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(ml_);
  }

  void set_mute(bool muted) {
    if (!ctx_ || !ml_) return;
    pa_threaded_mainloop_lock(ml_);
    if (pa_context_get_state(ctx_) == PA_CONTEXT_READY) {
      auto *op = pa_context_set_sink_mute_by_name(ctx_, "@DEFAULT_SINK@", muted ? 1 : 0, nullptr, nullptr);
      if (op) pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(ml_);
  }

 private:
  pa_threaded_mainloop *ml_{nullptr};
  pa_context *ctx_{nullptr};
};


// ============================================================================
// Player listener — blocking PA audio output
// ============================================================================

class PulsePlayerListener : public PlayerRoleListener {
 public:
  ~PulsePlayerListener() override { close_pa(true); }

  void set_player(PlayerRole *p) { player_ = p; }
  void set_volume_controller(PulseVolumeController *vc) { vol_ctrl_ = vc; }

  size_t on_audio_write(uint8_t *data, size_t length, uint32_t /*timeout_ms*/) override {
    if (length == 0) return 0;

    if (!pa_.load()) {
      int64_t now = now_us();
      if (now - last_open_attempt_us_ < backoff_us_) return length;
      last_open_attempt_us_ = now;

      pa_sample_spec ss{PA_SAMPLE_S16LE, 48000, 2};
      if (player_) {
        auto &params = player_->get_current_stream_params();
        if (params.sample_rate.has_value()) ss.rate = params.sample_rate.value();
        if (params.channels.has_value()) ss.channels = static_cast<uint8_t>(params.channels.value());
      }

      uint32_t bytes_per_frame = pa_frame_size(&ss);
      uint32_t tlength_bytes = ss.rate * bytes_per_frame * 20 / 1000;
      pa_buffer_attr ba{tlength_bytes * 2, tlength_bytes, 0, (uint32_t)-1, (uint32_t)-1};

      int err = 0;
      auto *pa = pa_simple_new(nullptr, "sendspin-client", PA_STREAM_PLAYBACK,
                               nullptr, "Sendspin", &ss, nullptr, &ba, &err);
      if (pa) {
        frame_size_ = bytes_per_frame;
        current_rate_ = ss.rate;
        current_channels_ = ss.channels;
        pa_.store(pa);
        backoff_us_ = kInitialBackoffUs;
        qCInfo(spclient) << "opened PA" << ss.rate << "Hz" << ss.channels << "ch" << "frame=" << frame_size_ << "tlength=" << tlength_bytes * 1000 / (ss.rate * bytes_per_frame);
      } else {
        qCInfo(spclient) << "pa_simple_new failed:" << pa_strerror(err);
        backoff_us_ = std::min(backoff_us_ * 2, kMaxBackoffUs);
        return length;
      }
    }

    auto *pa = pa_.load();
    size_t aligned = length - (length % frame_size_);
    if (aligned == 0) return length;

    // Software volume attenuation — mirrors MPV's internal volume behavior
    // so that both paths produce consistent loudness at the same volume%.
    // Uses square-root curve: gain = sqrt(vol/100) for a gentler rolloff.
    {
      auto *samples = reinterpret_cast<int16_t *>(data);
      size_t count = aligned / sizeof(int16_t);
      const int vol = last_volume_;
      if (g_ducked.load(std::memory_order_relaxed)) {
        // Duck: reduce to ~25% of current software volume
        const int32_t gain = static_cast<int32_t>(
            std::sqrt(vol / 100.0) * 0.25 * 256.0);
        for (size_t i = 0; i < count; ++i)
          samples[i] = static_cast<int16_t>((samples[i] * gain) >> 8);
      } else if (vol < 100) {
        const int32_t gain = static_cast<int32_t>(
            std::sqrt(vol / 100.0) * 256.0);  // 50% → gain=181 (~71%)
        for (size_t i = 0; i < count; ++i)
          samples[i] = static_cast<int16_t>((samples[i] * gain) >> 8);
      }
    }

    int err = 0;
    if (pa_simple_write(pa, data, aligned, &err) < 0) {
      qCWarning(spclient) <<  "pa_simple_write:" << pa_strerror(err);
      close_pa(false);
      return length;
    }

    uint32_t frames = static_cast<uint32_t>(aligned / frame_size_);
    int64_t latency_us = pa_simple_get_latency(pa, nullptr);
    if (latency_us < 0) latency_us = 0;
    player_->notify_audio_played(frames, now_us() + latency_us);
    return aligned;
  }

  void on_stream_start() override {
    qCDebug(spclient) << "on_stream_start";
    uint32_t new_rate = 0;
    uint8_t new_channels = 0;
    if (player_) {
      auto &p = player_->get_current_stream_params();
      if (p.sample_rate.has_value()) new_rate = *p.sample_rate;
      if (p.channels.has_value()) new_channels = static_cast<uint8_t>(*p.channels);
    }
    auto *pa = pa_.load();
    if (pa && new_rate == current_rate_ && new_channels == current_channels_) {
      pa_simple_flush(pa, nullptr);
    } else {
      close_pa(false);
    }
  }

  void on_stream_end() override {
    qCDebug(spclient) << "on_stream_end";
    close_pa(false);
  }

  void on_mute_changed(bool muted) override {
    qCDebug(spclient) << "on_mute_changed" << muted;
    if (vol_ctrl_) vol_ctrl_->set_mute(muted);
  }

  void on_volume_changed(uint8_t volume_percent) override {
    int percent = std::min((int)volume_percent, 100);
    last_volume_ = percent;
    if (vol_ctrl_) vol_ctrl_->set_volume_percent(percent);
    persist_volume(percent);
    qCDebug(spclient) << "on_volume_changed" << percent;
  }

  void on_static_delay_changed(uint16_t delay_ms) override {
    qCDebug(spclient) << "on_static_delay_changed" << delay_ms;
  }

  void sync_local_state(PlayerRole &player) {
    int percent = read_device_volume();
    if (percent < 0) return;
    if (percent != last_volume_) {
      player.update_volume(static_cast<uint8_t>(percent));
      last_volume_ = percent;
      qCDebug(spclient) << "sync_local_state" << percent;
    }
  }

 private:
  static constexpr int64_t kInitialBackoffUs = 100'000;
  static constexpr int64_t kMaxBackoffUs = 5'000'000;

  void close_pa(bool drain) {
    auto *pa = pa_.exchange(nullptr);
    if (pa) {
      if (drain) pa_simple_drain(pa, nullptr);
      else pa_simple_flush(pa, nullptr);
      pa_simple_free(pa);
      qCInfo(spclient) << "closed PulseAudio" << (drain ? "drain" : "flush");
    }
    current_rate_ = 0;
    current_channels_ = 0;
  }

  std::atomic<pa_simple *> pa_{nullptr};
  PlayerRole *player_{nullptr};
  PulseVolumeController *vol_ctrl_{nullptr};
  size_t frame_size_{4};
  uint32_t current_rate_{0};
  uint8_t current_channels_{0};
  int last_volume_{-1};
  int64_t last_open_attempt_us_{0};
  int64_t backoff_us_{kInitialBackoffUs};
};


// ============================================================================
// Controller listener — tracks playback state, suppresses LED on volume change
// ============================================================================

class ControllerListener : public ControllerRoleListener {
 public:
 
  void on_controller_state(const ServerStateControllerObject &state) override {
  qCInfo(spclient) << "Controller state changed vol=" << state.volume;
    last_vol_ = state.volume;
  }

  void on_controller_state_clear() override {
    qCInfo(spclient) << "Controller state cleared";
  }

 private:
  int last_vol_{-1};
};

// ============================================================================
// Metadata listener
// ============================================================================

class SimpleMetadataListener : public MetadataRoleListener {
 public:

  void on_metadata(const ServerMetadataStateObject &m) override {
    if (m.artist && m.title) {
      std::string track = *m.artist + " - " + *m.title;
      if (track != last_track_) {
        last_track_ = track;
        qCInfo(spclient) << "Now playing: " << track.c_str();
      }
    }
  }

  void on_metadata_clear() override {
    qCInfo(spclient) << "Metadata cleared";
    last_track_.clear();
  }

 private:
  std::string last_track_;
};

// ============================================================================
// Client listener — group state, high-performance networking
// ============================================================================

class MainClientListener : public SendspinClientListener {
 public:

  void on_time_sync_updated(float error) override {
    qCWarning(spclient) << "Time sync error: " << error << " us";
  }

  void on_group_update(const GroupUpdateObject &group) override {
    if (group.playback_state.has_value()) {
      auto state = *group.playback_state;
      qCInfo(spclient) << "Group playback state: " << (state == SendspinPlaybackState::PLAYING ? "playing" : "stopped");
  }
}

  void on_request_high_performance() override {
    qCInfo(spclient) << "High-performance requested";
  }
  void on_release_high_performance() override {
    qCInfo(spclient) << "High-performance released";
  }

};

class HostNetworkProvider : public SendspinNetworkProvider {
 public:
  bool is_network_ready() override { return true; }
};

SendSpinClientWrapper::SendSpinClientWrapper() = default;

SendSpinClientWrapper::~SendSpinClientWrapper() {
    stop();
}

bool SendSpinClientWrapper::start(const std::string& connect_url) {
    if (running_.load()) return false;
    running_.store(true);

    worker_thread_ = std::thread([this, connect_url]() {
        run_client(connect_url);
    });
    return true;
}

void SendSpinClientWrapper::stop() {
    if (!running_.load()) return;
    running_.store(false);

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void SendSpinClientWrapper::run_client(std::string connect_url) {
    std::string friendly_name = "KIOT " + QHostInfo::localHostName().toLower().toStdString();
    auto log_level = LogLevel::INFO;

    SendspinClient::set_log_level(log_level);

    SendspinClientConfig config;
    config.client_id = friendly_name;
    config.name = friendly_name;
    config.product_name = "KIOT Sendspin Client";
    config.manufacturer = "kiot";
    config.software_version = QStringLiteral(PLUGIN_VERSION).toStdString();

    SendspinClient client(std::move(config));

    FilePersistenceProvider persistence;
    client.set_persistence_provider(&persistence);

    PulseVolumeController vol_ctrl;
    
    PulsePlayerListener player_listener;
    PlayerRoleConfig player_config;
    player_config.audio_formats = {
        {SendspinCodecFormat::FLAC, 2, 44100, 16},
        {SendspinCodecFormat::FLAC, 2, 48000, 16},
        {SendspinCodecFormat::OPUS, 2, 48000, 16},
        {SendspinCodecFormat::PCM, 2, 44100, 16},
        {SendspinCodecFormat::PCM, 2, 48000, 16},
    };
    auto saved_delay = persistence.load_static_delay();
    if (saved_delay.has_value()) player_config.initial_static_delay_ms = *saved_delay;
    auto &player = client.add_player(std::move(player_config));
    player.set_static_delay_adjustable(true);
    player_listener.set_player(&player);
    player_listener.set_volume_controller(&vol_ctrl);
    player.set_listener(&player_listener);

    SimpleMetadataListener metadata_listener;
    client.add_metadata().set_listener(&metadata_listener);

    ControllerListener controller_listener;
    auto &controller = client.add_controller();
    controller.set_listener(&controller_listener);

    HostNetworkProvider network;
    MainClientListener client_listener;

    client.set_network_provider(&network);
    client.set_listener(&client_listener);

    client.start();
    qCInfo(spclient) << "Listening as " << friendly_name.c_str();
    if (!connect_url.empty()) client.connect_to(connect_url);

    std::atomic<int> local_tap_action{0};

    int poll_count = 0;
    while (running_.load()) {
        client.loop();
        
        int tap = local_tap_action.exchange(0);
        if (tap == 1) {
            if (client.get_group_state().playback_state == SendspinPlaybackState::PLAYING)
                controller.send_command({.command = SendspinControllerCommand::PAUSE});
            else
                controller.send_command({.command = SendspinControllerCommand::PLAY});
        }

        if (++poll_count >= 30) {
            poll_count = 0;
            player_listener.sync_local_state(player);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    qCInfo(spclient) << "Shutting down";
    client.disconnect(SendspinGoodbyeReason::SHUTDOWN);
}