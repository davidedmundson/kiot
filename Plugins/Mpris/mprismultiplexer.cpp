/*
    SPDX-FileCopyrightText: 2012 Alex Merry <alex.merry@kdemail.net>
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/
// TODO figure out if its okay to add my own name here under license as
// i copied most of the code from here
//  https://invent.kde.org/plasma/plasma-workspace/-/tree/master/libkmpris?ref_type=heads
// and stripped it to fit my needs

#include "mprismultiplexer.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <KIOTShared/kiotshared.h>
using KIOTShared::Entities::MediaPlayer;
using KIOTShared::PlatformHelper;
#include "playercontainer.h"

DEFINE_PLUGIN_LOGGER(mpris,Mpris)
MprisMultiplexer::MprisMultiplexer(QObject *parent)
    : QObject(parent)
{
    setupMediaPlayer();
    discoverPlayers();

    // Listen for new players appearing or stopping
    QDBusConnection::sessionBus().connect("org.freedesktop.DBus",
                                          "/org/freedesktop/DBus",
                                          "org.freedesktop.DBus",
                                          "NameOwnerChanged",
                                          this,
                                          SLOT(onNameOwnerChanged(QString, QString, QString)));
}

MprisMultiplexer::~MprisMultiplexer()
{
    // Slett alle spillere eksplisitt nå så D-Bus grensesnittene lukkes pent
    updateMediaPlayerEntity(nullptr);
    disconnect(m_playerEntity, nullptr, this, nullptr);
}

void MprisMultiplexer::setupMediaPlayer()
{
    m_playerEntity = new MediaPlayer(this);
    m_playerEntity->setId("mpris_media_player");
    m_playerEntity->setName("Kiot Active MPRIS Player");
    updateMediaPlayerEntity(nullptr); // To make sure any leftover state is cleared

    // Connect entity signals to player control methods
    connect(m_playerEntity, &MediaPlayer::playRequested, this, [this]() {
        if (m_activePlayer)
            m_activePlayer->Play();
    });
    connect(m_playerEntity, &MediaPlayer::pauseRequested, this, [this]() {
        if (m_activePlayer)
            m_activePlayer->Pause();
    });
    connect(m_playerEntity, &MediaPlayer::stopRequested, this, [this]() {
        if (m_activePlayer)
            m_activePlayer->Stop();
    });
    connect(m_playerEntity, &MediaPlayer::nextRequested, this, [this]() {
        if (m_activePlayer)
            m_activePlayer->Next();
    });
    connect(m_playerEntity, &MediaPlayer::previousRequested, this, [this]() {
        if (m_activePlayer)
            m_activePlayer->Previous();
    });
    connect(m_playerEntity, &MediaPlayer::volumeChanged, this, [this](double vol) {
        if (m_activePlayer)
            m_activePlayer->setVolume(vol);
    });
    connect(m_playerEntity, &MediaPlayer::positionChanged, this, [this](qint64 pos) {
        if (m_activePlayer)
            m_activePlayer->setPosition(pos);
    });
    connect(m_playerEntity, &MediaPlayer::playMediaRequested, this, [this](QString payload) {
        if (!m_activePlayer)
            return;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError) {
            qCWarning(mpris) << "JSON parse error:" << err.errorString();
            return;
        }
        QString mediaId = doc.object().value("media_id").toString();
        if (!mediaId.isEmpty())
            m_activePlayer->OpenUri(mediaId);
    });
}

void MprisMultiplexer::discoverPlayers()
{
    QDBusInterface dbusIface("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", QDBusConnection::sessionBus());
    QDBusPendingCall call = dbusIface.asyncCall("ListNames");
    auto watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        QDBusPendingReply<QStringList> reply = *watcher;
        watcher->deleteLater();
        if (!reply.isValid())
            return;
        for (const QString &svc : reply.value()) {
            if (svc.startsWith("org.mpris.MediaPlayer2."))
                addPlayer(svc);
        }
    });
}

void MprisMultiplexer::addPlayer(const QString &busName)
{
    qCDebug(mpris) << "Adding player:" << busName;
    auto *container = new PlayerContainer(busName, this);
    connect(container, &PlayerContainer::stateChanged, this, [this, container]() {
        handleActivePlayer(container);
    });
    m_containers.append(container);
}

void MprisMultiplexer::removePlayer(const QString &busName)
{
    auto it = std::find_if(m_containers.begin(), m_containers.end(), [&busName](PlayerContainer *c) {
        return c->busName() == busName;
    });

    if (it != m_containers.end()) {
        PlayerContainer *container = *it;

        qCDebug(mpris) << "Removing player:" << busName;

        // If this was the active player, clear it
        if (m_activePlayer == container) {
            m_activePlayer = nullptr;
            // Look for another player to become active
            PlayerContainer *newActive = nullptr;
            for (PlayerContainer *c : m_containers) {
                if (c != container) {
                    newActive = c;
                    break;
                }
            }

            if (newActive) {
                m_activePlayer = newActive;
                qCDebug(mpris) << "New active player:" << newActive->busName();
                updateMediaPlayerEntity(newActive);
            } else {
                updateMediaPlayerEntity(nullptr);
            }
        }

        m_containers.erase(it);
        container->deleteLater();
    }
}

void MprisMultiplexer::handleActivePlayer(PlayerContainer *container)
{
    PlaybackStatus::Status status = container->playbackStatus();

    if (status == PlaybackStatus::Playing) {
        if (m_activePlayer != container) {
            m_activePlayer = container;
            qCDebug(mpris) << "Active player changed to:" << container->busName();
        }
        updateMediaPlayerEntity(container);
        return;
    }

    if (m_activePlayer == container) {
        updateMediaPlayerEntity(container);

        PlayerContainer *playingPlayer = nullptr;
        for (PlayerContainer *c : m_containers) {
            if (c->playbackStatus() == PlaybackStatus::Playing) {
                playingPlayer = c;
                break;
            }
        }

        if (playingPlayer) {
            m_activePlayer = playingPlayer;
            qCDebug(mpris) << "Switched active player to:" << playingPlayer->busName();
            updateMediaPlayerEntity(playingPlayer);
        } else {
            updateMediaPlayerEntity(container);
        }
    } else if (!m_activePlayer && status != PlaybackStatus::Unknown) {
        m_activePlayer = container;
        qCDebug(mpris) << "Set initial active player:" << container->busName();
        updateMediaPlayerEntity(container);
    }
}

void MprisMultiplexer::downloadArt(const QString &url)
{
    if (m_artReply && m_artReply->isRunning()) {
        if (m_artReply->request().url() == QUrl(url)) {
            return;
        }
        m_artReply->abort();
    }
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
        m_network->setAutoDeleteReplies(true);
        m_network->setTransferTimeout(30000);
    }

    QNetworkReply *reply = m_network->get(QNetworkRequest(QUrl(url)));
    m_artReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() != QNetworkReply::OperationCanceledError) {
                qCWarning(mpris) << "Failed to download artwork:" << reply->errorString();
            }
            return;
        }
        // the player may have moved on to another track since the request
        if (m_playerEntity->state().value("art").toString() != url) {
            return;
        }
        m_playerEntity->setState({{QStringLiteral("albumart"), reply->readAll().toBase64()}});
    });
}

void MprisMultiplexer::updateMediaPlayerEntity(PlayerContainer *container)
{
    if (!container) {
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
    
    if (QCoreApplication::closingDown()) {
        qCDebug(mpris) << "Application is closing down, skipping update" << container->busName();
        return;
    }

    QVariantMap state;

    QString playbackStatusStr;
    switch (container->playbackStatus()) {
    case PlaybackStatus::Playing:
        playbackStatusStr = "Playing";
        break;
    case PlaybackStatus::Paused:
        playbackStatusStr = "Paused";
        break;
    case PlaybackStatus::Stopped:
        playbackStatusStr = "Stopped";
        break;
    default:
        playbackStatusStr = "Unknown";
        break;
    }

    state["state"] = playbackStatusStr;
    state["volume"] = container->volume();
    state["name"] = container->dbusname().replace("org.mpris.MediaPlayer2.", "");
    state["title"] = container->track();
    state["artist"] = container->artist();
    state["album"] = container->album();
    state["art"] = container->artUrl();
    state["position"] = static_cast<qint64>(container->position() / 1000000);
    state["duration"] = static_cast<qint64>(container->length() / 1000000.0);

    QString artUrl = state.value("art").toString();
    if (m_playerEntity->state()["art"] != state.value("art").toString()) {
        if (artUrl.startsWith("file://")) {
            QFile f(QUrl(artUrl).toLocalFile());
            if (f.open(QIODevice::ReadOnly))
                state["albumart"] = f.readAll().toBase64();
            else
                state["albumart"] = "";
        } else if (artUrl.startsWith("https://")) {
            qCDebug(mpris) << "Downloading artwork from" << artUrl;
            state["albumart"] = ""; // the image follows when downloaded, the old one belongs to another track
            downloadArt(artUrl);
        } else {
            state["albumart"] = "";
        }
    }

    m_playerEntity->setState(state);
}

void MprisMultiplexer::onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    if (!name.startsWith("org.mpris.MediaPlayer2."))
        return;

    if (!newOwner.isEmpty() && oldOwner.isEmpty()) {
        addPlayer(name);
    } else if (!oldOwner.isEmpty() && newOwner.isEmpty()) {
        removePlayer(name);
    }
}