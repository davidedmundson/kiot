/*
    SPDX-FileCopyrightText: 2012 Alex Merry <alex.merry@kdemail.net>
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/
// TODO figure out if this is okay as i copied code from https://invent.kde.org/plasma/plasma-workspace/-/tree/master/libkmpris?ref_type=heads
// and stripped it to fit my needs

/**
 * @file mpris.cpp
 * @brief MPRIS (Media Player Remote Interfacing Specification) integration for Kiot
 * 
 * This module integrates MPRIS-compatible media players with Home Assistant
 * through a custom MQTT media player entity. It monitors all MPRIS players
 * on the system and exposes the currently active player to Home Assistant
 * for control and monitoring.
 */

#include "core.h"
#include "entities/entities.h"

// Qt Core includes
#include <QObject>
#include <QFile>
#include <QTimer>
#include <QString>
#include <QVariantMap>
#include <QJsonObject>
#include <QEventLoop>

// Qt DBus includes
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDBusArgument>

// Qt Network includes
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(mpris)
Q_LOGGING_CATEGORY(mpris, "integration.MPRIS")

/**
 * @class PlayerContainer
 * @brief Container for a single MPRIS player instance
 * 
 * This class wraps a D-Bus connection to an MPRIS player and manages
 * its state, properties, and signal handling. It provides methods to
 * control the player and monitor its state changes.
 */
class PlayerContainer : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Construct a PlayerContainer for a specific MPRIS service
     * @param bus The D-Bus service name (e.g., "org.mpris.MediaPlayer2.spotify")
     * @param parent Parent QObject
     */
    explicit PlayerContainer(const QString &bus, QObject *parent = nullptr)
        : QObject(parent)
        , m_busName(bus)
    {
        // initial snapshot + start listening for changes
        refresh();
        // subscribe to PropertiesChanged signals for this service
        QDBusConnection::sessionBus().connect(
            m_busName,
            "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged",
            this,
            SLOT(onPropertiesChanged(QString,QVariantMap,QStringList))
        );
    }

    /**
     * @brief Destructor - cleans up D-Bus connections
     */
    ~PlayerContainer() override
    {
        // unsubscribe signal for cleanliness (optional)
        QDBusConnection::sessionBus().disconnect(
            m_busName,
            "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged",
            this,
            SLOT(onPropertiesChanged(QString,QVariantMap,QStringList))
        );
    }
    
    /// @return The D-Bus service name of this player
    QString busName() const { return m_busName; }
    
    /// Start playback
    void Play() { callMethod("Play"); }
    
    /// Pause playback
    void Pause() { callMethod("Pause"); }
    
    /// Stop playback
    void Stop() { callMethod("Stop"); }
    
    /// Skip to next track
    void Next() { callMethod("Next"); }
    
    /// Go to previous track
    void Previous() { callMethod("Previous"); }
    
    /**
     * @brief Set player volume
     * @param v Volume level (0.0 to 1.0)
     */
    void setVolume(double v) { setProperty("Volume", v); }
    
    /**
     * @brief Open a media URI for playback
     * @param uri Media URI to play
     */
    void OpenUri(const QString &uri){ callMethod("OpenUri",uri);   }
   
    /**
     * @brief Set the playback position to a specific time.
     * 
     * @param pos The target playback position in microseconds (qint64)
     * 
     * @note The pause/play workaround is necessary for proper position reporting in Home Assistant.
     * Without this workaround, the position will not be correctly updated in HA after seeking.
     * 
     * @see position() for getting the current playback position
     * @see stateChanged() signal emitted after position update
     */
    void setPosition(qint64 pos) { 

        QDBusInterface iface(m_busName, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", QDBusConnection::sessionBus());
        qint64 delta = pos - position();
        
        Pause();
        iface.call("Seek", QVariant::fromValue(delta));
        Play();
        
        m_state["Position"] = pos;
        emit stateChanged();
    }

    /// @return Current player state as a variant map
    QVariantMap state() const { return m_state; }
    
    /// @return D-Bus service name (alias for busName())
    QString dbusname() const { return m_busName; }
    
    /// @return Current playback position in microseconds
    qint64 position()
    {
        QDBusInterface iface(m_busName, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", QDBusConnection::sessionBus());
        auto value = iface.property("Position");

        return value.toLongLong();
    } 
    
signals:
    /// Emitted when player state changes
    void stateChanged();

private slots:
    /**
     * @brief Handle PropertiesChanged signals from D-Bus
     * @param interfaceName The interface that changed (unused)
     * @param changedProperties Map of changed properties
     * @param invalidatedProperties List of invalidated properties (unused)
     */
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &changedProperties, const QStringList &invalidatedProperties)
    {
        Q_UNUSED(interfaceName)
        Q_UNUSED(invalidatedProperties)

        // Merge changedProperties into m_state
        bool changed = false;
        for (auto it = changedProperties.cbegin(); it != changedProperties.cend(); ++it) {
            // For Metadata, it's a QDBusArgument; store raw QVariant so caller can decode
            if (it.key() == "Metadata") {
                m_state["Metadata"] = it.value();
                changed = true;
            } else if (!m_state.contains(it.key()) || m_state[it.key()] != it.value()) {
                m_state[it.key()] = it.value();
                changed = true;
            }
            
        }
        if (changed) {
            emit stateChanged();
        }
    }

private:
    /**
     * @brief Refresh player state by fetching all properties
     */
    void refresh()
    {
        QDBusInterface iface(m_busName, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", QDBusConnection::sessionBus());
        QDBusPendingCall call = iface.asyncCall("GetAll", "org.mpris.MediaPlayer2.Player");
        auto watcher = new QDBusPendingCallWatcher(call, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
            QDBusPendingReply<QVariantMap> reply = *watcher;
            watcher->deleteLater();
            if (reply.isValid()) {
                // store full state snapshot
                QVariantMap replyMap = reply.value();
                // Ensure Metadata stays as QDBusArgument
                m_state = replyMap;
                emit stateChanged();
            }
        });
    }

    
    /**
     * @brief Call a method on the MPRIS player
     * @param method Method name to call
     * @param args Optional arguments for the method
     */
    void callMethod(const QString &method,const QString &args=QString())
    {
        QDBusInterface iface(m_busName, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", QDBusConnection::sessionBus());
        if(!args.isEmpty()) //The args was just a workaround for OpenUrl and as it worked i kept it
            iface.call(method,args);
        else
            iface.call(method);
    }

    /**
     * @brief Set a property on the MPRIS player
     * @param prop Property name to set
     * @param val New property value
     */
    void setProperty(const QString &prop, const QVariant &val)
    {
        QDBusInterface iface(m_busName, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", QDBusConnection::sessionBus());
        iface.call("Set", "org.mpris.MediaPlayer2.Player", prop, QVariant::fromValue(QDBusVariant(val)));
    }

    QString m_busName;        ///< D-Bus service name
    QVariantMap m_state;      ///< Current player state

};


/**
 * @class MprisMultiplexer
 * @brief Manages multiple MPRIS players and exposes the active one to Home Assistant
 * 
 * This class monitors all MPRIS players on the system, selects the active
 * player (preferring playing ones), and exposes it as a single MediaPlayer
 * to Home Assistant. It handles player discovery, removal, and state updates.
 */
class MprisMultiplexer : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Construct the MPRIS multiplexer
     * @param parent Parent QObject
     */
    explicit MprisMultiplexer(QObject *parent = nullptr) : QObject(parent)
    {
        setupMediaPlayer();
        discoverPlayers();


        // Listen for new players appearing or stopping
        QDBusConnection::sessionBus().connect(
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "NameOwnerChanged",
            this,
            SLOT(onNameOwnerChanged(QString,QString,QString))
        );
    }

    /**
     * @brief Desctructor to set mediaplayer as off in HA
     */
    ~MprisMultiplexer()
    {
        updateMediaPlayerEntity(nullptr);
    }
private:

    /**
     * @brief Set up the MediaPlayer entity for Home Assistant
     */
    void setupMediaPlayer()
    {
        m_playerEntity = new MediaPlayer(this);
        m_playerEntity->setId("mpris_media_player");
        m_playerEntity->setName("Kiot Active MPRIS Player");

        // Connect entity signals to player control methods
        connect(m_playerEntity,&MediaPlayer::playRequested, this, [this]() {
            if(m_activePlayer) m_activePlayer->Play();
        });
        connect(m_playerEntity,&MediaPlayer::pauseRequested, this, [this]() {
            if(m_activePlayer) m_activePlayer->Pause();
        });
        connect(m_playerEntity,&MediaPlayer::stopRequested, this, [this]() {
            if(m_activePlayer) m_activePlayer->Stop();
        });
        connect(m_playerEntity,&MediaPlayer::nextRequested, this, [this]() {
            if(m_activePlayer) m_activePlayer->Next();
        });
        connect(m_playerEntity,&MediaPlayer::previousRequested, this, [this]() {
            if(m_activePlayer) m_activePlayer->Previous();
        });
        connect(m_playerEntity,&MediaPlayer::volumeChanged, this, [this](double vol){
            if(m_activePlayer) m_activePlayer->setVolume(vol);
        });
        connect(m_playerEntity, &MediaPlayer::positionChanged, this, [this](qint64 pos){
            if(m_activePlayer) m_activePlayer->setPosition(pos);
        });
        connect(m_playerEntity,&MediaPlayer::playMediaRequested, this, [this](QString payload){
            if(!m_activePlayer) return;
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &err);
            if(err.error != QJsonParseError::NoError){
                qCWarning(mpris) << "JSON parse error:" << err.errorString();
                return;
            }
            QString mediaId = doc.object().value("media_id").toString();
            if(!mediaId.isEmpty()) m_activePlayer->OpenUri(mediaId);
        });
    }

    /**
     * @brief Discover all MPRIS players currently running on the system
     */
    void discoverPlayers()
    {
        QDBusInterface dbusIface("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", QDBusConnection::sessionBus());
        QDBusPendingCall call = dbusIface.asyncCall("ListNames");
        auto watcher = new QDBusPendingCallWatcher(call, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher](){
            QDBusPendingReply<QStringList> reply = *watcher;
            watcher->deleteLater();
            if(!reply.isValid()) return;
            for(const QString &svc : reply.value()){
                if(svc.startsWith("org.mpris.MediaPlayer2.")) addPlayer(svc);
            }
        });
    }

    /**
     * @brief Add a new MPRIS player to the multiplexer
     * @param busName D-Bus service name of the player
     */
    void addPlayer(const QString &busName)
    {
        qCDebug(mpris) << "Adding player:" << busName;
        auto *container = new PlayerContainer(busName, this);
        connect(container, &PlayerContainer::stateChanged, this, [this, container](){
            handleActivePlayer(container);
        });
        m_containers.append(container);
    }

    /**
     * @brief Remove an MPRIS player from the multiplexer
     * @param busName D-Bus service name of the player to remove
     * 
     * Handles cleanup and switching to another player if the active one is removed.
     */
    void removePlayer(const QString &busName)
    {
        auto it = std::find_if(m_containers.begin(), m_containers.end(), 
            [&busName](PlayerContainer *c){ return c->busName() == busName; });
        
        if (it != m_containers.end()) {
            PlayerContainer *container = *it;
            
            qCDebug(mpris) << "Removing player:" << busName;
            
            // If this was the active player, clear it
            if (m_activePlayer == container) {
                m_activePlayer = nullptr;
                // Look for another player to become active
                PlayerContainer *newActive = nullptr;
                for (PlayerContainer *c : m_containers) {
                    if (c != container) { // Skip the one being removed
                        newActive = c;
                        break;
                    }
                }
                
                if (newActive) {
                    m_activePlayer = newActive;
                    qCDebug(mpris) << "New active player:" << newActive->busName();
                    updateMediaPlayerEntity(newActive);
                } else {
                    // No players left
                    updateMediaPlayerEntity(nullptr);
                }
            }
            
            // Remove from list and delete
            m_containers.erase(it);
            container->deleteLater();
        }
    }

    /**
     * @brief Handle state changes from a player and update active player selection
     * @param container The player whose state changed
     * 
     * Active player selection logic:
     * 1. Playing players have highest priority
     * 2. If active player stops, look for another playing player
     * 3. If no players are playing, keep current active player
     */
    void handleActivePlayer(PlayerContainer *container)
    {
        const QString status = container->state().value("PlaybackStatus").toString();
        
        if (status == "Playing") {
            // This player is playing, make it active
            if (m_activePlayer != container) {
                m_activePlayer = container;
                qCDebug(mpris) << "Active player changed to:" << container->busName();
            }
            updateMediaPlayerEntity(container);
            return;
        }
        
        // If this was the active player but stopped/paused
        if (m_activePlayer == container) {
            updateMediaPlayerEntity(container);
            
            // Look for another playing player
            PlayerContainer *playingPlayer = nullptr;
            for (PlayerContainer *c : m_containers) {
                if (c->state().value("PlaybackStatus").toString() == "Playing") {
                    playingPlayer = c;
                    break;
                }
            }
            
            if (playingPlayer) {
                // Switch to another playing player
                m_activePlayer = playingPlayer;
                qCDebug(mpris) << "Switched active player to:" << playingPlayer->busName();
                updateMediaPlayerEntity(playingPlayer);
            } else {
                // No player is playing, keep current as active but show paused/stopped
                updateMediaPlayerEntity(container);
            }
        } else if (!m_activePlayer && container->state().contains("PlaybackStatus")) {
            // No active player yet, use this one
            m_activePlayer = container;
            qCDebug(mpris) << "Set initial active player:" << container->busName();
            updateMediaPlayerEntity(container);
        }
    }
    
    /**
     * @brief Download artwork from a URL and convert to Base64
     * @param url URL of the artwork to download
     * @return Base64-encoded image data, or empty string on failure
     */
    QString downloadArtAsBase64(const QString &url)
    {
        QNetworkAccessManager manager;
        QNetworkRequest request(url);
        QNetworkReply *reply = manager.get(request);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        QByteArray data;
        if(reply->error() == QNetworkReply::NoError) {
            data = reply->readAll();
        } else {
            qCWarning(mpris) << "Failed to download artwork:" << reply->errorString();
        }

        reply->deleteLater();
        return data.toBase64();
    }

    /**
     * @brief Update the MediaPlayer with state from a player container
     * @param container Player container to get state from, or nullptr for empty state
     * 
     * @details
     * This method converts MPRIS player state to Home Assistant media player entity state.
     * It handles metadata extraction, artwork downloading, and empty state when no player is active.
     */
    void updateMediaPlayerEntity(PlayerContainer *container)
    {
        if (!container) {
            // No active player, set empty state
            QVariantMap emptyState;
            emptyState["state"] = "off";
            emptyState["volume"] = 0.0;
            emptyState["name"] = "";
            emptyState["title"] = "";
            emptyState["artist"] = "";
            emptyState["album"] = "";
            emptyState["art"] = "";
            emptyState["position"] = 0;
            emptyState["duration"] = 0;
            emptyState["albumart"] = "";
            m_playerEntity->setState(emptyState);
            return;
        }
        
        const auto &cState = container->state();
        QVariantMap state;
        state["state"] = cState.value("PlaybackStatus","Stopped").toString();
        state["volume"] = cState.value("Volume",1.0).toDouble();
        state["name"] = container->dbusname().replace("org.mpris.MediaPlayer2.","");
        qint64 pos = container->position()/1000000;
        qint64 dur = 0;
        if(cState.contains("Metadata")){
            QVariantMap metadata;
            QDBusArgument arg = cState.value("Metadata").value<QDBusArgument>();
            arg >> metadata;

            state["title"] = metadata.value("xesam:title").toString();
            QVariant artistVal = metadata.value("xesam:artist");
            state["artist"] = artistVal.canConvert<QStringList>() ? artistVal.toStringList().join(", ") : artistVal.toString();
            QVariant albumVal = metadata.value("xesam:album");
            state["album"] = albumVal.canConvert<QStringList>() ? albumVal.toStringList().join(", ") : albumVal.toString();
            QVariant artVal = metadata.value("mpris:artUrl");
            state["art"] = artVal.toString();
            if(metadata.contains("mpris:length")) dur = metadata.value("mpris:length").toLongLong()/1000000;
        }
        
        state["position"] = pos;
        state["duration"] = dur;

        // Artwork -> Base64
        QString artUrl = state.value("art").toString();
        if(m_playerEntity->state()["art"] != state.value("art").toString()){
            if(artUrl.startsWith("file://")){
                QString path = artUrl.mid(QString("file://").length());
                QFile f(path);
                if(f.open(QIODevice::ReadOnly)) state["albumart"] = f.readAll().toBase64();
            }
            else if(artUrl.startsWith("https://")){
                qCDebug(mpris) << "Downloading artwork from" << artUrl;
                state["albumart"] = downloadArtAsBase64(artUrl);
            }
            else{
                state["albumart"] = "";
            }
        }

        m_playerEntity->setState(state);
    }
    
    QList<PlayerContainer*> m_containers;         ///< List of all discovered MPRIS players
    PlayerContainer *m_activePlayer = nullptr;    ///< Currently active player (playing or selected)
    MediaPlayer *m_playerEntity;            ///< Home Assistant media player entity
    
private slots:
    /**
     * @brief Handle D-Bus NameOwnerChanged signals for MPRIS player discovery/removal
     * @param name D-Bus service name that changed ownership
     * @param oldOwner Previous owner of the service name
     * @param newOwner New owner of the service name
     * 
     * This slot monitors D-Bus for MPRIS players appearing (newOwner != "") or
     * disappearing (newOwner == ""). It ensures the multiplexer always reflects
     * the current set of available media players.
     */
    void onNameOwnerChanged(const QString &name,const QString &oldOwner,const QString &newOwner)
    {
        if(!name.startsWith("org.mpris.MediaPlayer2.")) return;

        if(!newOwner.isEmpty() && oldOwner.isEmpty()) {
            // New player appeared
            addPlayer(name);
        } else if(!oldOwner.isEmpty() && newOwner.isEmpty()){
            // Player disappeared
            removePlayer(name);
        }
    }
};


/**
 * @brief Initialize the MPRIS integration
 * 
 * This function is called by the integration factory
 */
void setupMprisIntegration()
{
    new MprisMultiplexer(qApp);
}

/**
 * @brief Register the MPRIS integration with Kiot
 * 
 */
REGISTER_INTEGRATION("MPRISPlayer", setupMprisIntegration, false)

#include "mpris.moc"