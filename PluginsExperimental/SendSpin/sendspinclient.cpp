#include "sendspinclient.h"
#include <sendspin/color_role.h>
#include <sendspin/controller_role.h>
#include <sendspin/metadata_role.h>
#include <sendspin/player_role.h>
#include <sendspin/visualizer_role.h>

#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <cmath>
#include <iostream>

using namespace sendspin;
class DesktopPersistenceProvider : public SendspinPersistenceProvider {
public:
    DesktopPersistenceProvider() {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(configDir);
        path_ = configDir + "/sendspin.json";

        // Sørg for at filen eksisterer med standardverdier hvis den ikke finnes fra før
        QFile file(path_);
        if (!file.exists() || file.size() == 0) {
            QJsonObject obj;
            obj["last_server_hash"] = 0;
            obj["static_delay_ms"] = 0;
            
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(obj).toJson());
            }
        }
    }

    bool save_last_server_hash(uint32_t hash) override {
        bool success = writeField("last_server_hash", static_cast<int>(hash));
        if (success) {
            qCInfo(spclient) << "persisted last_server_hash:" << hash;
        }
        return success;
    }

    std::optional<uint32_t> load_last_server_hash() override {
        int val = readField("last_server_hash");
        if (val < 0) return std::nullopt;
        return static_cast<uint32_t>(val);
    }

    bool save_static_delay(uint16_t delay_ms) override {
        bool success = writeField("static_delay_ms", delay_ms);
        if (success) {
            qCInfo(spclient) << "persisted static_delay:" << delay_ms;
        }
        return success;
    }

    std::optional<uint16_t> load_static_delay() override {
        int val = readField("readField" == QString("static_delay_ms") ? "static_delay_ms" : "static_delay_ms"); // (liten skriveleif-sjekk fix under)
        // Riktig linje:
        // int val = readField("static_delay_ms");
        if (val < 0) return std::nullopt;
        return static_cast<uint16_t>(val);
    }

private:
    QString path_;

    int readField(const QString &field) {
        QFile file(path_);
        if (!file.open(QIODevice::ReadOnly)) return -1;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject()) return -1;
        return doc.object().value(field).toInt(-1);
    }

    bool writeField(const QString &field, int value) {
        QJsonObject obj;
        QFile file(path_);
        if (file.open(QIODevice::ReadOnly)) {
            obj = QJsonDocument::fromJson(file.readAll()).object();
            file.close();
        }
        
        obj[field] = value;
        
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(obj).toJson());
            return true;
        }
        return false;
    }
};
class DesktopPlayerListener : public PlayerRoleListener {
public:
    DesktopPlayerListener(SendspinDesktopClient *parent) : parent_(parent) {}
    ~DesktopPlayerListener() override { parent_->closeAudio(); }

    size_t on_audio_write(uint8_t *data, size_t length, uint32_t) override {
        if (length == 0) return 0;

        if (!parent_->pa_.load()) {
            uint32_t rate = 48000;
            uint8_t channels = 2;
            if (player_) {
                auto &params = player_->get_current_stream_params();
                if (params.sample_rate.has_value()) rate = *params.sample_rate;
                if (params.channels.has_value()) channels = static_cast<uint8_t>(*params.channels);
            }
            parent_->initAudio(rate, channels);
        }

        auto *pa = parent_->pa_.load();
        if (!pa) return length;

        size_t aligned = length - (length % parent_->frame_size_);
        if (aligned == 0) return length;

        int vol = parent_->current_volume_;
        if (vol < 100) {
            auto *samples = reinterpret_cast<int16_t *>(data);
            size_t count = aligned / sizeof(int16_t);
            const int32_t gain = static_cast<int32_t>(std::sqrt(vol / 100.0) * 256.0);
            for (size_t i = 0; i < count; ++i) {
                samples[i] = static_cast<int16_t>((samples[i] * gain) >> 8);
            }
        }

        int err = 0;
        if (pa_simple_write(pa, data, aligned, &err) < 0) {
            std::fprintf(stderr, "pa_simple_write error: %s\n", pa_strerror(err));
            parent_->closeAudio();
            return length;
        }

        uint32_t frames = static_cast<uint32_t>(aligned / parent_->frame_size_);
        int64_t latency_us = pa_simple_get_latency(pa, nullptr);
        if (latency_us < 0) latency_us = 0;

        auto now_us = []() {
            return std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        };
        player_->notify_audio_played(frames, now_us() + latency_us);
        return aligned;
    }

    void set_player(PlayerRole *p) { player_ = p; }
    void on_volume_changed(uint8_t volume_percent) override {
        parent_->current_volume_ = std::min(static_cast<int>(volume_percent), 100);
        emit parent_->volumeChanged(parent_->current_volume_);
    }

private:
    SendspinDesktopClient *parent_;
    PlayerRole *player_{nullptr};
};

class DesktopMetadataListener : public MetadataRoleListener {
public:
    DesktopMetadataListener(SendspinDesktopClient *parent) : parent_(parent) {}
    void on_metadata(const ServerMetadataStateObject &m) override {
        if (m.artist && m.title) {
            emit parent_->trackChanged(QString::fromStdString(*m.artist), QString::fromStdString(*m.title));
        }
    }
private:
    SendspinDesktopClient *parent_;
};

SendspinDesktopClient::SendspinDesktopClient(const QString &clientName, QObject *parent)
: QObject(parent) {

    SendspinClientConfig config;
    config.client_id = clientName.toStdString();
    config.name = clientName.toStdString();
    config.product_name = QStringLiteral(PROJECT_NAME).toStdString() + " " + QStringLiteral(PLUGIN_NAME).toStdString();
    config.manufacturer = QStringLiteral(PROJECT_NAME).toStdString();
    config.software_version = QStringLiteral(PLUGIN_VERSION).toStdString();

    client_ = std::make_unique<SendspinClient>(std::move(config));

    persistence_ = new DesktopPersistenceProvider();
    client_->set_persistence_provider(persistence_);

    player_listener_ = new DesktopPlayerListener(this);
    PlayerRoleConfig player_config;
    player_config.audio_formats = {
        {SendspinCodecFormat::FLAC, 2, 44100, 16},
        {SendspinCodecFormat::FLAC, 2, 48000, 16},
        {SendspinCodecFormat::PCM, 2, 44100, 16},
        {SendspinCodecFormat::PCM, 2, 48000, 16},
    };
    auto &player = client_->add_player(std::move(player_config));
    player_listener_->set_player(&player);
    player.set_listener(player_listener_);

    auto *meta_listener = new DesktopMetadataListener(this);
    auto &metadata = client_->add_metadata();
    metadata.set_listener(meta_listener);

    client_->start_server();

    QTimer *loopTimer = new QTimer(this);
    connect(loopTimer, &QTimer::timeout, this, [this]() {
        if (client_) client_->loop();
    });
        loopTimer->start(20);
}

SendspinDesktopClient::~SendspinDesktopClient() {
    closeAudio();
    delete persistence_;
}

void SendspinDesktopClient::connectToServer(const QString &url) {
    if (client_) client_->connect_to(url.toStdString());
}

void SendspinDesktopClient::disconnectFromServer() {
    if (client_) client_->disconnect(SendspinGoodbyeReason::SHUTDOWN);
}

void SendspinDesktopClient::setVolume(int percent) {
    current_volume_ = std::clamp(percent, 0, 100);
    emit volumeChanged(current_volume_);
}

int SendspinDesktopClient::volume() const {
    return current_volume_;
}

void SendspinDesktopClient::initAudio(uint32_t rate, uint8_t channels) {
    pa_sample_spec ss{PA_SAMPLE_S16LE, rate, channels};
    frame_size_ = pa_frame_size(&ss);
    current_rate_ = rate;
    current_channels_ = channels;

    uint32_t tlength_bytes = rate * frame_size_ * 20 / 1000;
    pa_buffer_attr ba{tlength_bytes * 2, tlength_bytes, 0, (uint32_t)-1, (uint32_t)-1};

    int err = 0;
    pa_simple *pa = pa_simple_new(nullptr, "kde-sendspin-client", PA_STREAM_PLAYBACK,
                                  nullptr, "Sendspin Desktop", &ss, nullptr, &ba, &err);
    if (pa) {
        pa_.store(pa);
    } else {
        std::fprintf(stderr, "[QtSendspin] pa_simple_new failed: %s\n", pa_strerror(err));
    }
}

void SendspinDesktopClient::closeAudio() {
    auto *pa = pa_.exchange(nullptr);
    if (pa) {
        pa_simple_drain(pa, nullptr);
        pa_simple_free(pa);
    }
}
