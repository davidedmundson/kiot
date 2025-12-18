#include "core.h"
#include "mediaplayer.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QMqttClient>
#include <QMqttSubscription>
#include <QDebug>

MediaPlayer::MediaPlayer(QObject *parent)
    : Entity(parent)
{
    setHaType("media_player");
}

void MediaPlayer::init()
{

    setDiscoveryConfig("name",name());
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("state_state_topic",  baseTopic() + "/state");
    setDiscoveryConfig("state_title_topic", baseTopic() + "/title");
    setDiscoveryConfig("state_artist_topic", baseTopic() + "/artist");
    setDiscoveryConfig("state_album_topic", baseTopic() + "/album");
    setDiscoveryConfig("state_duration_topic", baseTopic() + "/duration");
    setDiscoveryConfig("state_position_topic", baseTopic() + "/position");
    setDiscoveryConfig("state_volume_topic", baseTopic() + "/volume");
    setDiscoveryConfig("state_albumart_topic", baseTopic() + "/albumart");
    setDiscoveryConfig("state_mediatype_topic", baseTopic() + "/mediatype");
    setDiscoveryConfig("command_play_topic", baseTopic() + "/play");
    setDiscoveryConfig("command_pause_topic", baseTopic() + "/playpause");
    setDiscoveryConfig("command_playpause_topic", baseTopic() + "/mediatype");
    setDiscoveryConfig("command_next_topic", baseTopic() + "/next");
    setDiscoveryConfig("command_previous_topic", baseTopic() + "/previous");
    setDiscoveryConfig("command_volume_topic", baseTopic() + "/set_volume");
    setDiscoveryConfig("command_playmedia_topic", baseTopic() + "/playmedia");
    setDiscoveryConfig("command_seek_position_topic", baseTopic() + "/setposition");
    sendRegistration();
    //subscriptions
    auto mqtt = HaControl::mqttClient();

    auto subscribe = [mqtt, this](const QString &topic, auto slot) {
        auto subscription = mqtt->subscribe(topic);
        if(subscription){
            connect(subscription, &QMqttSubscription::messageReceived, this, [this, slot](const QMqttMessage &msg){
                //qCDebug() << "Message received on topic" << msg.topic() << "payload:" << msg.payload();
                const QString payload = QString::fromUtf8(msg.payload());
                (this->*slot)(payload);
            });
        }
    };

    subscribe(baseTopic() + "/play", &MediaPlayer::onPlayCommand);
    subscribe(baseTopic() + "/pause", &MediaPlayer::onPauseCommand);
    subscribe(baseTopic() + "/playpause", &MediaPlayer::onPlayPauseCommand);
    subscribe(baseTopic() + "/stop", &MediaPlayer::onStopCommand);
    subscribe(baseTopic() + "/next", &MediaPlayer::onNextCommand);
    subscribe(baseTopic() + "/previous", &MediaPlayer::onPreviousCommand);
    subscribe(baseTopic() + "/set_volume", &MediaPlayer::onSetVolumeCommand);
    subscribe(baseTopic() + "/playmedia", &MediaPlayer::onPlayMediaCommand);
    subscribe(baseTopic() + "/setposition", &MediaPlayer::onPositionCommand);
}


void MediaPlayer::setState(const QVariantMap &info)
{
    bool changed = false;

    for (auto it = info.begin(); it != info.end(); ++it) {
        if (!m_state.contains(it.key()) || m_state[it.key()] != it.value()) {
            m_state[it.key()] = it.value();
            changed = true;
        }
    }

    if (changed) {
        emit stateChanged(m_state);
        publishState(); // Hvis du har en publishState implementasjon
    }
}

QVariantMap MediaPlayer::state() const
{
    return m_state;
}


// --- Slots for commands ---
void MediaPlayer::onPlayCommand(const QString &) { play(); }
void MediaPlayer::onPauseCommand(const QString &) { pause(); }
void MediaPlayer::onPlayPauseCommand(const QString &payload) { 
    if(payload == "Pause")
        pause();
    else if(payload == "Play")
        play();
}
void MediaPlayer::onStopCommand(const QString &) { stop(); }
void MediaPlayer::onNextCommand(const QString &) { next(); }
void MediaPlayer::onPreviousCommand(const QString &) { previous(); }
void MediaPlayer::onSetVolumeCommand(const QString &payload) { setVolume(payload.toDouble()); }
void MediaPlayer::onPlayMediaCommand(const QString &payload) { emit playMediaRequested(payload); }
void MediaPlayer::onPositionCommand(const QString &payload) { emit positionChanged( static_cast<qint64>(payload.toDouble() * 1000000 )); }

// --- Public slots ---
void MediaPlayer::play() { 
    m_state["state"] = "playing"; 
    emit playRequested(); 
    publishState();
}
void MediaPlayer::pause() { 
    m_state["state"] = "paused"; 
    emit pauseRequested(); 
    publishState();
}
void MediaPlayer::stop() { 
    m_state["state"] = "stopped"; 
    emit stopRequested(); 
    publishState();
}
void MediaPlayer::next() { emit nextRequested(); }
void MediaPlayer::previous() { emit previousRequested(); }
void MediaPlayer::setVolume(double volume) { 
    m_state["volume"] = volume; 
    emit volumeChanged(volume); 
    publishState();
}

// --- Publish current state to all HA topics ---
void MediaPlayer::publishState()
{
    auto mqtt = HaControl::mqttClient();
    if(m_state.value("name").toString() != name())
       {
        setName(m_state["name"].toString());
        sendRegistration();
       }

    mqtt->publish(baseTopic() + "/state", m_state.value("state").toString().toLower().toUtf8(), 0, true);
    mqtt->publish(baseTopic() + "/title",    m_state.value("title").toString().toUtf8(), 0, true);
    mqtt->publish(baseTopic() + "/artist", m_state.value("artist").toString().toUtf8(), 0, true);
    mqtt->publish(baseTopic() + "/album", m_state.value("album").toString().toUtf8(), 0, true);
    mqtt->publish(baseTopic() + "/duration", QByteArray::number(m_state.value("duration").toInt()), 0, true);
    mqtt->publish(baseTopic() + "/position", QByteArray::number(m_state.value("position").toInt()), 0, true);
    mqtt->publish(baseTopic() + "/volume", QByteArray::number(m_state.value("volume").toDouble()), 0, true);
    mqtt->publish(baseTopic() + "/albumart", m_state.value("albumart").toByteArray(), 0, true);
    mqtt->publish(baseTopic() + "/mediatype", m_state.value("mediatype").toString().toUtf8(), 0, true);
 
}
