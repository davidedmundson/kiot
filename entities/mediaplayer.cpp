#include "core.h"
#include "mediaplayer.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QMqttClient>
#include <QMqttSubscription>
#include <QDebug>

MediaPlayerEntity::MediaPlayerEntity(QObject *parent)
    : Entity(parent)
{
    setHaType("media_player");
}

void MediaPlayerEntity::init()
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

    subscribe(baseTopic() + "/play", &MediaPlayerEntity::onPlayCommand);
    subscribe(baseTopic() + "/pause", &MediaPlayerEntity::onPauseCommand);
    subscribe(baseTopic() + "/playpause", &MediaPlayerEntity::onPlayPauseCommand);
    subscribe(baseTopic() + "/stop", &MediaPlayerEntity::onStopCommand);
    subscribe(baseTopic() + "/next", &MediaPlayerEntity::onNextCommand);
    subscribe(baseTopic() + "/previous", &MediaPlayerEntity::onPreviousCommand);
    subscribe(baseTopic() + "/set_volume", &MediaPlayerEntity::onSetVolumeCommand);
    subscribe(baseTopic() + "/playmedia", &MediaPlayerEntity::onPlayMediaCommand);
}

void MediaPlayerEntity::setState(const QVariantMap &info)
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

QVariantMap MediaPlayerEntity::state() const
{
    return m_state;
}

void MediaPlayerEntity::setAvailablePlayers(const QStringList &players)
{
    m_players = players;
}

QStringList MediaPlayerEntity::availablePlayers() const
{
    return m_players;
}

// --- Slots for commands ---
void MediaPlayerEntity::onPlayCommand(const QString &) { play(); }
void MediaPlayerEntity::onPauseCommand(const QString &) { pause(); }
void MediaPlayerEntity::onPlayPauseCommand(const QString &payload) { 
    if(payload == "Pause")
        pause();
    else if(payload == "Play")
        play();
}
void MediaPlayerEntity::onStopCommand(const QString &) { stop(); }
void MediaPlayerEntity::onNextCommand(const QString &) { next(); }
void MediaPlayerEntity::onPreviousCommand(const QString &) { previous(); }
void MediaPlayerEntity::onSetVolumeCommand(const QString &payload) { setVolume(payload.toDouble()); }
void MediaPlayerEntity::onPlayMediaCommand(const QString &payload) { emit playMediaRequested(payload); }

// --- Public slots ---
void MediaPlayerEntity::play() { 
    m_state["state"] = "playing"; 
    emit playRequested(); 
    publishState();
}
void MediaPlayerEntity::pause() { 
    m_state["state"] = "paused"; 
    emit pauseRequested(); 
    publishState();
}
void MediaPlayerEntity::stop() { 
    m_state["state"] = "stopped"; 
    emit stopRequested(); 
    publishState();
}
void MediaPlayerEntity::next() { emit nextRequested(); }
void MediaPlayerEntity::previous() { emit previousRequested(); }
void MediaPlayerEntity::setVolume(double volume) { 
    m_state["volume"] = volume; 
    emit volumeChanged(volume); 
    publishState();
}

// --- Publish current state to all HA topics ---
void MediaPlayerEntity::publishState()
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
