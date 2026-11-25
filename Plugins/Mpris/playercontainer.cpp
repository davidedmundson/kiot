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

#include "playercontainer.h"
#include <KIOTShared/kiotshared.h>

using KIOTShared::Entities::MediaPlayer;
using KIOTShared::PlatformHelper;

// Qt Core includes
#include <QEventLoop>
#include <QFile>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QSettings>
#include <QUrl>

// Qt DBus includes
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusObjectPath>

// Include generated DBus interface headers
#include "dbusproperties.h"
#include "mprisplayer.h"
#include "mprisroot.h"

// Logger definisjon (forutsetter at denne er tilgjengelig i modulen)
Q_DECLARE_LOGGING_CATEGORY(mpris)

PlayerContainer::PlayerContainer(const QString &bus, QObject *parent)
    : AbstractPlayerContainer(parent)
    , m_busName(bus)
    , m_propsIface(new OrgFreedesktopDBusPropertiesInterface(bus, MPRIS2_PATH, QDBusConnection::sessionBus(), this))
    , m_playerIface(new OrgMprisMediaPlayer2PlayerInterface(bus, MPRIS2_PATH, QDBusConnection::sessionBus(), this))
    , m_rootIface(new OrgMprisMediaPlayer2Interface(bus, MPRIS2_PATH, QDBusConnection::sessionBus(), this))
{
    Q_ASSERT(bus.startsWith(QLatin1String("org.mpris.MediaPlayer2")));
    initBindings();
    refresh();
    if (QDBusReply<unsigned> pidReply = QDBusConnection::sessionBus().interface()->servicePid(bus); pidReply.isValid()) {
        m_instancePid = pidReply.value();
    }
}

PlayerContainer::~PlayerContainer()
{
    if (m_propsIface) {
        disconnect(m_propsIface, nullptr, this, nullptr);
    }
    if (m_playerIface) {
        disconnect(m_playerIface, nullptr, this, nullptr);
    }
}

QString PlayerContainer::busName() const
{
    return m_busName;
}

QString PlayerContainer::dbusname() const
{
    return m_busName;
}

void PlayerContainer::Play()
{
    Q_ASSERT_X(m_canPlay.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
    if (!m_canPlay.value()) {
        return;
    }
    m_playerIface->Play();
}

void PlayerContainer::Pause()
{
    Q_ASSERT_X(m_canPause.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
    if (!m_canPause.value()) {
        return;
    }
    m_playerIface->Pause();
}

void PlayerContainer::Stop()
{
    Q_ASSERT_X(m_canStop.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
    if (!m_canStop.value()) {
        return;
    }
    m_playerIface->Stop();
}

void PlayerContainer::Next()
{
    Q_ASSERT_X(m_canGoNext.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
    if (!m_canGoNext.value()) {
        return;
    }
    m_playerIface->Next();
}

void PlayerContainer::Previous()
{
    Q_ASSERT_X(m_canGoPrevious.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
    if (!m_canGoPrevious.value()) {
        return;
    }
    m_playerIface->Previous();
}

void PlayerContainer::setVolume(double value)
{
    if (m_volume == value) {
        return;
    }
    m_propsIface->Set(QStringLiteral("org.mpris.MediaPlayer2.Player"), QStringLiteral("Volume"), QDBusVariant(QVariant(value)));
}

void PlayerContainer::OpenUri(const QString &Uri)
{
    m_playerIface->OpenUri(Uri);
}

void PlayerContainer::seek(qlonglong pos)
{
    qlonglong delta = pos - position();

    Q_ASSERT_X(m_canSeek.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
    if (!m_canSeek.value()) {
        return;
    }
    m_playerIface->Seek(delta);
    m_position = pos;
    updatePosition();
}

void PlayerContainer::setPosition(qlonglong value)
{
    if (!m_canSeek.value()) {
        qCWarning(mpris) << m_busName << "can't seek, ignoring the position";
        return;
    }
    if (m_trackId.value().isEmpty()) {
        qCWarning(mpris) << m_busName << "gives no track id, can't set the position";
        return;
    }

    m_playerIface->SetPosition(QDBusObjectPath(m_trackId.value()), value);
    m_position = value;
    updatePosition();
}

void PlayerContainer::onSeeked(qlonglong position)
{
    m_position = position;
    Q_EMIT stateChanged();
}

void PlayerContainer::onGetPropsFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QVariantMap> propsReply = *watcher;
    watcher->deleteLater();

    if (m_fetchesPending < 1) {
        qCWarning(mpris) << "Got a reply for a fetch that was already failed";
        Q_EMIT initialFetchFailed(this);
        return;
    }

    if (propsReply.isError()) {
        qCDebug(mpris) << m_busName << "does not implement" << OrgFreedesktopDBusPropertiesInterface::staticInterfaceName() << "correctly"
                       << "Error message was" << propsReply.error().name() << propsReply.error().message();
        m_fetchesPending = 0;
        Q_EMIT initialFetchFailed(this);
        return;
    }

    updateFromMap(propsReply.value());

    if (--m_fetchesPending == 0) {
        if (m_identity.isEmpty()) {
            qCDebug(mpris) << "MPRIS2 service" << objectName() << "isn't standard-compliant, ignoring";
            Q_EMIT initialFetchFailed(this);
            return;
        }

        Q_EMIT initialFetchFinished(this);
        Q_EMIT stateChanged();
        connect(m_propsIface, &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, &PlayerContainer::onPropertiesChanged);
        connect(m_playerIface, &OrgMprisMediaPlayer2PlayerInterface::Seeked, this, &PlayerContainer::onSeeked);
    }
}

void PlayerContainer::onPropertiesChanged(const QString &interfaceName, const QVariantMap &changedProperties, const QStringList &invalidatedProperties)
{
    if (!invalidatedProperties.empty() || interfaceName == u"org.mpris.MediaPlayer2.TrackList") {
        disconnect(m_propsIface, &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, &PlayerContainer::onPropertiesChanged);
        disconnect(m_playerIface, &OrgMprisMediaPlayer2PlayerInterface::Seeked, this, &PlayerContainer::onSeeked);
        refresh();
    } else if (interfaceName == u"org.mpris.MediaPlayer2.Player" || interfaceName == u"org.mpris.MediaPlayer2") [[likely]] {
        updateFromMap(changedProperties);
    }
}

QString PlayerContainer::getDesktopFileIcon(const QString &filePath)
{
    QString desktopFile = filePath;
    if (!desktopFile.endsWith(QLatin1String(".desktop"))) {
        desktopFile += QStringLiteral(".desktop");
    }

    QSettings desktop(desktopFile, QSettings::IniFormat);
    desktop.beginGroup(QStringLiteral("Desktop Entry"));
    QString icon = desktop.value(QStringLiteral("Icon")).toString();
    desktop.endGroup();

    return icon;
}

void PlayerContainer::updateFromMap(const QVariantMap &map)
{
    auto updateSingleProperty = [this]<typename T>(T &property, const QVariant &value, QMetaType::Type expectedType, void (PlayerContainer::*signal)()) {
        if (value.metaType().id() != expectedType) {
            qCWarning(mpris) << m_busName << "exports" << value.metaType() << "but it should be" << QMetaType(expectedType);
        }
        if (T newProperty = value.value<T>(); property != newProperty) {
            property = newProperty;
            Q_EMIT(this->*signal)();
            Q_EMIT stateChanged();
        }
    };

    QString oldTrackId;

    for (auto it = map.cbegin(); it != map.cend(); it = std::next(it)) {
        const QString &propName = it.key();

        if (propName == QLatin1String("Identity")) {
            updateSingleProperty(m_identity, it.value(), QMetaType::QString, &PlayerContainer::identityChanged);
        } else if (propName == QLatin1String("DesktopEntry")) {
            if (QString iconName = getDesktopFileIcon(it.value().toString() + QLatin1String(".desktop")); !iconName.isEmpty()) {
                m_iconName = std::move(iconName);
            }
            updateSingleProperty(m_desktopEntry, it.value(), QMetaType::QString, &PlayerContainer::desktopEntryChanged);
        } else if (propName == QLatin1String("SupportedUriSchemes")) {
            updateSingleProperty(m_supportedUriSchemes, it.value(), QMetaType::QStringList, &PlayerContainer::supportedUriSchemesChanged);
        } else if (propName == QLatin1String("SupportedMimeTypes")) {
            updateSingleProperty(m_supportedMimeTypes, it.value(), QMetaType::QStringList, &PlayerContainer::supportedMimeTypesChanged);
        } else if (propName == QLatin1String("Fullscreen")) {
            m_fullscreen = it->toBool();
        } else if (propName == QLatin1String("HasTrackList")) {
            m_hasTrackList = it->toBool();
        } else if (propName == QLatin1String("PlaybackStatus")) {
            if (const QString newValue = it->toString(); newValue == QLatin1String("Stopped")) {
                m_playbackStatus = PlaybackStatus::Stopped;
            } else if (newValue == QLatin1String("Paused")) {
                m_playbackStatus = PlaybackStatus::Paused;
            } else if (newValue == QLatin1String("Playing")) {
                m_playbackStatus = PlaybackStatus::Playing;
            } else {
                m_playbackStatus = PlaybackStatus::Unknown;
            }
        } else if (propName == QLatin1String("LoopStatus")) {
            if (const QString newValue = it.value().toString(); newValue == QLatin1String("Playlist")) {
                m_loopStatus = LoopStatus::Playlist;
            } else if (newValue == QLatin1String("Track")) {
                m_loopStatus = LoopStatus::Track;
            } else {
                m_loopStatus = LoopStatus::None;
            }
        } else if (propName == QLatin1String("Shuffle")) {
            m_shuffle = it->toBool() ? ShuffleStatus::On : ShuffleStatus::Off;
        } else if (propName == QLatin1String("Rate")) {
            m_rate = it->toDouble();
        } else if (propName == QLatin1String("MinimumRate")) {
            m_minimumRate = it->toDouble();
        } else if (propName == QLatin1String("MaximumRate")) {
            m_maximumRate = it->toDouble();
        } else if (propName == QLatin1String("Volume")) {
            m_volume = it->toDouble();
        } else if (propName == QLatin1String("Position")) {
            m_position = it->toLongLong();
        } else if (propName == QLatin1String("Metadata")) {
            oldTrackId = m_trackId.value();
            auto arg = it->value<QDBusArgument>();
            if (arg.currentType() != QDBusArgument::MapType || arg.currentSignature() != QLatin1String("a{sv}")) {
                continue;
            }

            QVariantMap innerMap;
            arg >> innerMap;

            if (auto metaDataIt = innerMap.constFind(QStringLiteral("mpris:trackid")); metaDataIt != innerMap.cend()) [[likely]] {
                if (metaDataIt->metaType() == QMetaType::fromType<QDBusObjectPath>()) {
                    m_trackId = qvariant_cast<QDBusObjectPath>(*metaDataIt).path();
                } else {
                    qCDebug(mpris) << "mpris:trackid from" << m_identity
                                   << "does not conform to the MPRIS2 standard.";
                    m_trackId = metaDataIt->toString();
                }
            } else {
                m_trackId = QString();
            }
            m_xesamTitle = innerMap[QStringLiteral("xesam:title")].toString();
            m_xesamUrl = innerMap[QStringLiteral("xesam:url")].toString();
            m_xesamArtist = innerMap[QStringLiteral("xesam:artist")].toStringList();
            m_xesamAlbumArtist = innerMap[QStringLiteral("xesam:albumArtist")].toStringList();
            m_xesamAlbum = innerMap[QStringLiteral("xesam:album")].toString();
            m_artUrl = innerMap[QStringLiteral("mpris:artUrl")].toString();
            m_length = innerMap[QStringLiteral("mpris:length")].toDouble();
            m_kdePid = innerMap[QStringLiteral("kde:pid")].toUInt();
        } else if (propName == QLatin1String("CanControl")) {
            m_canControl = it->toBool();
        } else if (propName == QLatin1String("CanSeek")) {
            m_canSeek = it->toBool();
        } else if (propName == QLatin1String("CanGoNext")) {
            m_canGoNext = it->toBool();
        } else if (propName == QLatin1String("CanGoPrevious")) {
            m_canGoPrevious = it->toBool();
        } else if (propName == QLatin1String("CanRaise")) {
            m_canRaise = it->toBool();
        } else if (propName == QLatin1String("CanSetFullscreen")) {
            m_canSetFullscreen = it->toBool();
        } else if (propName == QLatin1String("CanQuit")) {
            m_canQuit = it->toBool();
        } else if (propName == QLatin1String("CanPlay")) {
            m_canPlay = it->toBool();
        } else if (propName == QLatin1String("CanPause")) {
            m_canPause = it->toBool();
        }
    }

    if (map.contains(QStringLiteral("Position"))) {
        return;
    }

    if (m_position > 0 && (m_playbackStatus == PlaybackStatus::Stopped || (!oldTrackId.isEmpty() && m_trackId.value() != oldTrackId))) {
        updatePosition();
    }
    Q_EMIT stateChanged();
}

void PlayerContainer::initBindings()
{
    m_effectiveCanGoNext.setBinding([this] {
        return m_canControl.value() && m_canGoNext.value();
    });
    m_effectiveCanGoPrevious.setBinding([this] {
        return m_canControl.value() && m_canGoPrevious.value();
    });
    m_effectiveCanPlay.setBinding([this] {
        return m_canControl.value() && m_canPlay.value();
    });
    m_effectiveCanPause.setBinding([this] {
        return m_canControl.value() && m_canPause.value();
    });
    m_effectiveCanStop.setBinding([this] {
        return m_canControl.value() && m_canStop.value();
    });
    m_effectiveCanSeek.setBinding([this] {
        return m_canControl.value() && m_canSeek.value();
    });

    m_canStop.setBinding([this] {
        return m_canControl.value() && m_playbackStatus.value() > PlaybackStatus::Stopped;
    });

    m_track.setBinding([this] {
        if (!m_xesamTitle.value().isEmpty()) {
            return m_xesamTitle.value();
        }
        const QStringView xesamUrl{m_xesamUrl.value()};
        if (xesamUrl.isEmpty()) {
            return QString();
        }
        if (int lastSlashPos = xesamUrl.lastIndexOf(QLatin1Char('/')); lastSlashPos < 0 || lastSlashPos == xesamUrl.size() - 1) {
            return QString();
        } else {
            const QStringView lastUrlPart = xesamUrl.sliced(lastSlashPos + 1);
            return QUrl::fromPercentEncoding(lastUrlPart.toLatin1());
        }
    });

    m_artist.setBinding([this] {
        if (!m_xesamArtist.value().empty()) {
            return m_xesamArtist.value().join(QLatin1String(", "));
        }
        if (!m_xesamAlbumArtist.value().empty()) {
            return m_xesamAlbumArtist.value().join(QLatin1String(", "));
        }
        return QString();
    });

    m_album.setBinding([this] {
        if (!m_xesamAlbum.value().isEmpty()) {
            return m_xesamAlbum.value();
        }
        const QStringView xesamUrl{m_xesamUrl.value()};
        if (!xesamUrl.startsWith(QLatin1String("file:///"))) {
            return QString();
        }
        const QList<QStringView> urlParts = xesamUrl.split(QLatin1Char('/'));
        if (urlParts.size() < 3) {
            return QString();
        }
        if (auto lastFolderPathIt = std::next(urlParts.crbegin()); !lastFolderPathIt->isEmpty()) {
            return QUrl::fromPercentEncoding(lastFolderPathIt->toLatin1());
        }
        return QString();
    });

    auto callback = [this] {
        updatePosition();
    };
    m_rateNotifier = m_rate.addNotifier(callback);
    m_playbackStatusNotifier = m_playbackStatus.addNotifier(callback);
}

void PlayerContainer::refresh()
{
    QDBusPendingCall async = m_propsIface->GetAll(QStringLiteral("org.mpris.MediaPlayer2"));
    auto watcher = new QDBusPendingCallWatcher(async, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &PlayerContainer::onGetPropsFinished);
    ++m_fetchesPending;

    async = m_propsIface->GetAll(QStringLiteral("org.mpris.MediaPlayer2.Player"));
    watcher = new QDBusPendingCallWatcher(async, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &PlayerContainer::onGetPropsFinished);
    ++m_fetchesPending;
    Q_EMIT stateChanged();
}

void PlayerContainer::updatePosition()
{
    QDBusPendingCall call = m_propsIface->Get(QStringLiteral("org.mpris.MediaPlayer2.Player"), QStringLiteral("Position"));
    auto watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QVariant> propsReply = *watcher;
        watcher->deleteLater();
        if (!propsReply.isValid() && propsReply.error().type() != QDBusError::NotSupported) {
            qCDebug(mpris) << m_busName << "does not implement" << OrgFreedesktopDBusPropertiesInterface::staticInterfaceName()
                           << "correctly. Error message was" << propsReply.error().name() << propsReply.error().message();
            return;
        }

        m_position = propsReply.value().toLongLong();
        Q_EMIT stateChanged();
    });
}

