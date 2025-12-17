/*
    SPDX-FileCopyrightText: 2012 Alex Merry <alex.merry@kdemail.net>
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/
// TODO figure out if this is okay as i copied code from https://invent.kde.org/plasma/plasma-workspace/-/tree/master/libkmpris?ref_type=heads
// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/entities.h"

// Qt Core includes
#include <QObject>
#include <QFile>
#include <QTimer>
#include <QString>
#include <QVariantMap>
#include <QJsonObject>
#include <QDebug>
#include <QEventLoop>
#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(mpris)
Q_LOGGING_CATEGORY(mpris, "integration.MPRIS")
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
class PlayerContainer : public QObject
{
    Q_OBJECT
public:
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
    QString busName() const { return m_busName; }
    void Play() { callMethod("Play"); }
    void Pause() { callMethod("Pause"); }
    void Stop() { callMethod("Stop"); }
    void Next() { callMethod("Next"); }
    void Previous() { callMethod("Previous"); }
    void setVolume(double v) { setProperty("Volume", v); }
    void OpenUri(const QString &uri){ callMethod("OpenUri",uri);   }
    QVariantMap state() const { return m_state; }
    QString dbusname() const { return m_busName; }

signals:
    void stateChanged();

private slots:
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
            } else {
                if (!m_state.contains(it.key()) || m_state[it.key()] != it.value()) {
                    m_state[it.key()] = it.value();
                    changed = true;
                }
            }
        }
        if (changed) {
            emit stateChanged();
        }
    }

private:
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
                // Ensure Metadata stays as QDBusArgument if present (GetAll returns a{sv}, already suitable)
                m_state = replyMap;
                emit stateChanged();
            }
        });
    }

    void callMethod(const QString &method,const QString &args=QString())
    {
        QDBusInterface iface(m_busName, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", QDBusConnection::sessionBus());
        if(!args.isEmpty())
            iface.call(method,args);
        else
            iface.call(method);
    }

    void setProperty(const QString &prop, const QVariant &val)
    {
        QDBusInterface iface(m_busName, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", QDBusConnection::sessionBus());
        iface.call("Set", "org.mpris.MediaPlayer2.Player", prop, QVariant::fromValue(QDBusVariant(val)));
    }

    QString m_busName;
    QVariantMap m_state;
};


class MprisMultiplexer : public QObject
{
    Q_OBJECT
public:
    explicit MprisMultiplexer(QObject *parent = nullptr) : QObject(parent)
    {
        setupMediaPlayer();
        discoverPlayers();

        // Timer for å oppdatere posisjon kontinuerlig
        m_positionTimer = new QTimer(this);
        connect(m_positionTimer, &QTimer::timeout, this, &MprisMultiplexer::updatePosition);
        m_positionTimer->start(1000); // hver 0,5s
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

private:
    void updatePosition()
    {
        if (!m_activePlayer) return;

        QDBusInterface iface(m_activePlayer->busName(), "/org/mpris/MediaPlayer2",
                             "org.mpris.MediaPlayer2.Player", QDBusConnection::sessionBus());
        QDBusReply<qlonglong> reply = iface.call("Position");
        if (reply.isValid()) {
            QVariantMap state = m_playerEntity->state();
            state["position"] = static_cast<qint64>(reply.value() / 1000000); // µs → sek
            m_playerEntity->setState(state);
        }
    }
    void setupMediaPlayer()
    {
        m_playerEntity = new MediaPlayerEntity(this);
        m_playerEntity->setId("kiotprisstate");
        m_playerEntity->setName("Kiot Active MPRIS Player");

        connect(m_playerEntity, &MediaPlayerEntity::playRequested, this, [this]() {
            if(m_activePlayer) m_activePlayer->Play();
        });
        connect(m_playerEntity, &MediaPlayerEntity::pauseRequested, this, [this]() {
            if(m_activePlayer) m_activePlayer->Pause();
        });
        connect(m_playerEntity, &MediaPlayerEntity::stopRequested, this, [this]() {
            if(m_activePlayer) m_activePlayer->Stop();
        });
        connect(m_playerEntity, &MediaPlayerEntity::nextRequested, this, [this]() {
            if(m_activePlayer) m_activePlayer->Next();
        });
        connect(m_playerEntity, &MediaPlayerEntity::previousRequested, this, [this]() {
            if(m_activePlayer) m_activePlayer->Previous();
        });
        connect(m_playerEntity, &MediaPlayerEntity::volumeChanged, this, [this](double vol){
            if(m_activePlayer) m_activePlayer->setVolume(vol);
        });
        connect(m_playerEntity, &MediaPlayerEntity::playMediaRequested, this, [this](QString payload){
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

    void addPlayer(const QString &busName)
    {
        qCDebug(mpris) << "Adding player:" << busName;
        auto *container = new PlayerContainer(busName, this);
        connect(container, &PlayerContainer::stateChanged, this, [this, container](){
            handleActivePlayer(container);
        });
        m_containers.append(container);
    }

    void handleActivePlayer(PlayerContainer *container)
    {
        const QString status = container->state().value("PlaybackStatus").toString();
        if(status == "Playing"){
            if(m_activePlayer != container) m_activePlayer = container;
            updateMediaPlayerEntity(container);
            return;
        }
        if(m_activePlayer == container) updateMediaPlayerEntity(container);
        if(!m_activePlayer && container->state().contains("PlaybackStatus")){
            m_activePlayer = container;
            updateMediaPlayerEntity(container);
        }
    }
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
    void updateMediaPlayerEntity(PlayerContainer *container)
    {
        const auto &cState = container->state();
        QVariantMap state;
        state["state"] = cState.value("PlaybackStatus","Stopped").toString();
        state["volume"] = cState.value("Volume",1.0).toDouble();
        state["name"] = m_activePlayer->dbusname().replace("org.mpris.MediaPlayer2.","");
        qint64 pos = cState.value("Position",0).toLongLong()/1000000;
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
        if(cState.contains("Duration") && dur==0) dur = cState.value("Duration").toLongLong()/1000000;
        //qCDebug(mpris) << "Position:" << pos << "Duration:" << dur;
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
    QTimer *m_positionTimer;
    QList<PlayerContainer*> m_containers;
    PlayerContainer *m_activePlayer = nullptr;
    MediaPlayerEntity *m_playerEntity;
private slots:
    void onNameOwnerChanged(const QString &name,const QString &oldOwner,const QString &newOwner)
    {
        if(!name.startsWith("org.mpris.MediaPlayer2.")) return;

        if(!newOwner.isEmpty() && oldOwner.isEmpty()) addPlayer(name);
        else if(!oldOwner.isEmpty() && newOwner.isEmpty()){
            auto it = std::find_if(m_containers.begin(), m_containers.end(), [&name](PlayerContainer *c){ return c->objectName() == name; });
            if(it != m_containers.end()){
                if(m_activePlayer == *it) m_activePlayer = nullptr;
                (*it)->deleteLater();
                m_containers.erase(it);
            }
        }
    }

};


void setupMprisIntegration()
{
    new MprisMultiplexer(qApp);
}

REGISTER_INTEGRATION("MPRISPlayer", setupMprisIntegration, false)

#include "mpris.moc"
