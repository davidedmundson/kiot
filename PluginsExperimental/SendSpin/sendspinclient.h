#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
#include <sendspin/client.h>
#include <pulse/simple.h>

class SendspinDesktopClient : public QObject {
    Q_OBJECT
public:
    explicit SendspinDesktopClient(const QString &clientName = "kiot", QObject *parent = nullptr);
    ~SendspinDesktopClient();

    void connectToServer(const QString &url);
    void disconnectFromServer();

    void setVolume(int percent);
    int volume() const;

signals:
    void trackChanged(const QString &artist, const QString &title);
    void playbackStateChanged(bool playing);
    void volumeChanged(int percent);

private:
    friend class DesktopPlayerListener;
    friend class DesktopPersistenceProvider;

    std::unique_ptr<sendspin::SendspinClient> client_;
    class DesktopPlayerListener *player_listener_{nullptr};
    class DesktopPersistenceProvider *persistence_{nullptr};

    std::atomic<pa_simple *> pa_{nullptr};
    int current_volume_{100};
    size_t frame_size_{4};
    uint32_t current_rate_{44100};
    uint8_t current_channels_{2};

    void initAudio(uint32_t rate, uint8_t channels);
    void closeAudio();
};
