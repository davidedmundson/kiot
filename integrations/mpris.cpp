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

/**
 * @file mpris.cpp
 * @brief MPRIS (Media Player Remote Interfacing Specification) integration for Kiot
 * 
 * This module integrates MPRIS-compatible media players with Home Assistant
 * through a custom MQTT media player entity. It monitors all MPRIS players
 * on the system and exposes the currently active player to Home Assistant
 * for control and monitoring.
 */
#include "mpris.h"
#include "core.h"
#include "entities/mediaplayer.h"

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

#include <QDBusObjectPath>


// Qt Network includes
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>


#include <KDesktopFile>
#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(mpris)
Q_LOGGING_CATEGORY(mpris, "integration.MPRIS")

// Include generated DBus interface headers
#include "dbusproperties.h"
#include "mprisplayer.h"
#include "mprisroot.h"


AbstractPlayerContainer::AbstractPlayerContainer(QObject *parent)
    : QObject(parent)
{
}

AbstractPlayerContainer::~AbstractPlayerContainer() = default;

bool AbstractPlayerContainer::canControl() const
{
    return m_canControl.value();
}

bool AbstractPlayerContainer::canGoNext() const
{
    return m_effectiveCanGoNext.value();
}

bool AbstractPlayerContainer::canGoPrevious() const
{
    return m_effectiveCanGoPrevious.value();
}

bool AbstractPlayerContainer::canPause() const
{
    return m_effectiveCanPause.value();
}

bool AbstractPlayerContainer::canPlay() const
{
    return m_effectiveCanPlay.value();
}

bool AbstractPlayerContainer::canStop() const
{
    return m_effectiveCanStop.value();
}

bool AbstractPlayerContainer::canSeek() const
{
    return m_effectiveCanSeek.value();
}

LoopStatus::Status AbstractPlayerContainer::loopStatus() const
{
    return m_loopStatus.value();
}

double AbstractPlayerContainer::maximumRate() const
{
    return m_maximumRate.value();
}

double AbstractPlayerContainer::minimumRate() const
{
    return m_minimumRate.value();
}

PlaybackStatus::Status AbstractPlayerContainer::playbackStatus() const
{
    return m_playbackStatus.value();
}

qlonglong AbstractPlayerContainer::position() const
{
    return m_position.value();
}

double AbstractPlayerContainer::rate() const
{
    return m_rate.value();
}

ShuffleStatus::Status AbstractPlayerContainer::shuffle() const
{
    return m_shuffle.value();
}

double AbstractPlayerContainer::volume() const
{
    return m_volume.value();
}

QString AbstractPlayerContainer::track() const
{
    return m_track.value();
}

QString AbstractPlayerContainer::artist() const
{
    return m_artist.value();
}

QString AbstractPlayerContainer::artUrl() const
{
    return m_artUrl.value();
}

QString AbstractPlayerContainer::album() const
{
    return m_album.value();
}

double AbstractPlayerContainer::length() const
{
    return m_length;
}

unsigned AbstractPlayerContainer::instancePid() const
{
    return m_instancePid;
}

unsigned AbstractPlayerContainer::kdePid() const
{
    return m_kdePid.value();
}

bool AbstractPlayerContainer::canQuit() const
{
    return m_canQuit.value();
}

bool AbstractPlayerContainer::canRaise() const
{
    return m_canRaise.value();
}

bool AbstractPlayerContainer::canSetFullscreen() const
{
    return m_canSetFullscreen.value();
}

QString AbstractPlayerContainer::desktopEntry() const
{
    return m_desktopEntry;
}

bool AbstractPlayerContainer::fullscreen() const
{
    return m_fullscreen.value();
}
bool AbstractPlayerContainer::hasTrackList() const
{
    return m_hasTrackList;
}

QString AbstractPlayerContainer::identity() const
{
    return m_identity;
}

QStringList AbstractPlayerContainer::supportedMimeTypes() const
{
    return m_supportedMimeTypes;
}

QStringList AbstractPlayerContainer::supportedUriSchemes() const
{
    return m_supportedUriSchemes;
}

QString AbstractPlayerContainer::iconName() const
{
    return m_iconName;
}


/**
 * @class PlayerContainer
 * @brief Container for a single MPRIS player instance
 * 
 * This class wraps a D-Bus connection to an MPRIS player and manages
 * its state, properties, and signal handling. It provides methods to
 * control the player and monitor its state changes.
 */
class PlayerContainer : public AbstractPlayerContainer
{
    Q_OBJECT
public:
    /**
     * @brief Construct a PlayerContainer for a specific MPRIS service
     * @param bus The D-Bus service name (e.g., "org.mpris.MediaPlayer2.spotify")
     * @param parent Parent QObject
     */
    explicit PlayerContainer(const QString &bus, QObject *parent = nullptr)
        : AbstractPlayerContainer(parent)  // FIKSET: Kaller riktig baseklassekonstruktør
        , m_busName(bus)
        , m_propsIface(new OrgFreedesktopDBusPropertiesInterface(bus, MPRIS2_PATH, QDBusConnection::sessionBus(), this))
        , m_playerIface(new OrgMprisMediaPlayer2PlayerInterface(bus, MPRIS2_PATH, QDBusConnection::sessionBus(), this))
        , m_rootIface(new OrgMprisMediaPlayer2Interface(bus, MPRIS2_PATH, QDBusConnection::sessionBus(), this))  
    {
        Q_ASSERT(bus.startsWith("org.mpris.MediaPlayer2"));
        initBindings();
        refresh();
        if (QDBusReply<unsigned> pidReply = QDBusConnection::sessionBus().interface()->servicePid(bus); pidReply.isValid()) {
            m_instancePid = pidReply.value();
        }
    }

    /**
     * @brief Destructor
     */
    ~PlayerContainer() = default;

    /// @return The D-Bus service name of this player
    QString busName() const { return m_busName; }
    
    /// Start playback
    void Play() { 
        Q_ASSERT_X(m_canPlay.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
        if (!m_canPlay.value()) {
            return;
        }
        m_playerIface->Play();
    }
    
    /// Pause playback
    void Pause() { 
        Q_ASSERT_X(m_canPause.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
        if (!m_canPause.value()) {
            return;
        }
        m_playerIface->Pause();
    }
    
    /// Stop playback
    void Stop() { 
        Q_ASSERT_X(m_canStop.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
        if (!m_canStop.value()) {
            return;
        }
        m_playerIface->Stop();
    }

    
    
    /// Skip to next track
    void Next() {
        Q_ASSERT_X(m_canGoNext.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
        if (!m_canGoNext.value()) {
            return;
        }
        m_playerIface->Next();
    }
    
    /// Go to previous track
    void Previous() {
        Q_ASSERT_X(m_canGoPrevious.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
        if (!m_canGoPrevious.value()) {
            return;
        }
        m_playerIface->Previous();
    }
    
    /**
     * @brief Set player volume
     * @param value Volume level (0.0 to 1.0)
     */
    void setVolume(double value) { 
        if (m_volume == value) {
            return;
        }

        m_propsIface->Set(QStringLiteral("org.mpris.MediaPlayer2.Player"), QStringLiteral("Volume"), QDBusVariant(QVariant(value)));
    }

    
    
    /**
     * @brief Open a media URI for playback
     * @param uri Media URI to play
     */
    void OpenUri(const QString &Uri){ 
        m_playerIface->OpenUri(Uri);
    }
   
    /**
     * @brief Set the playback position to a specific time.
     * 
     * @param pos The target playback position in microseconds (qint64)
     * 
     * @note The pause/play workaround is necessary for proper position reporting in Home Assistant.
     * Without this workaround, the position will not be correctly updated in HA after seeking.
     * 
     * @see position() for getting the current playback position
     * @see stateChanged() signal Q_EMITted after position update
     */
    void seek(qlonglong pos) { 
        qlonglong delta = pos - position();
        
        Q_ASSERT_X(m_canSeek.value(), Q_FUNC_INFO, qUtf8Printable(identity()));
        if (!m_canSeek.value()) {
            return;
        }
        m_playerIface->Seek(delta);
        m_position = pos;
        updatePosition();
    }
    void setPosition(qlonglong value)
    {
        if (m_position == value) {
            return;
        }

        m_playerIface->SetPosition(QDBusObjectPath(m_trackId.value()), value);
        m_position = value;
        updatePosition();
    }
    /// @return D-Bus service name (alias for busName())
    QString dbusname() const { return m_busName; }
    
Q_SIGNALS:
    /// Q_EMITted when player state changes
    void stateChanged();
    void initialFetchFinished(PlayerContainer *container);
    void initialFetchFailed(PlayerContainer *container);

private Q_SLOTS:
    void onSeeked(qlonglong position)
    {
        m_position = position;
        Q_EMIT stateChanged();
    }
    
    void onGetPropsFinished(QDBusPendingCallWatcher *watcher)
    {
        QDBusPendingReply<QVariantMap> propsReply = *watcher;
        watcher->deleteLater();

        if (m_fetchesPending < 1) {
            // we already failed
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
            // Check if the player follows the specification dutifully.
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

    /**
     * @brief Handle PropertiesChanged signals from D-Bus
     * @param interfaceName The interface that changed (unused)
     * @param changedProperties Map of changed properties
     * @param invalidatedProperties List of invalidated properties (unused)
     */
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &changedProperties, const QStringList &invalidatedProperties)
    {
        if (!invalidatedProperties.empty() || interfaceName == u"org.mpris.MediaPlayer2.TrackList") {
            disconnect(m_propsIface, &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, &PlayerContainer::onPropertiesChanged);
            disconnect(m_playerIface, &OrgMprisMediaPlayer2PlayerInterface::Seeked, this, &PlayerContainer::onSeeked);
            refresh();
        } else if (interfaceName == u"org.mpris.MediaPlayer2.Player" || interfaceName == u"org.mpris.MediaPlayer2") [[likely]] {
            updateFromMap(changedProperties);
        }
    }

private:

    void updateFromMap(const QVariantMap &map)
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
                if (QString iconName = KDesktopFile(it.value().toString() + QLatin1String(".desktop")).readIcon(); !iconName.isEmpty()) {
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

                QVariantMap map;
                arg >> map;

                if (auto metaDataIt = map.constFind(QStringLiteral("mpris:trackid")); metaDataIt != map.cend()) [[likely]] {
                    if (metaDataIt->metaType() == QMetaType::fromType<QDBusObjectPath>()) {
                        m_trackId = get<QDBusObjectPath>(*metaDataIt).path();
                    } else {
                        // BUG 482603: work around nonstandard players like Spotify
                        qCDebug(mpris) << "mpris:trackid from" << m_identity
                                        << "does not conform to the MPRIS2 standard. Please report the "
                                           "issue to the developer.";
                        m_trackId = metaDataIt->toString();
                    }
                } else {
                    m_trackId = QString();
                }
                m_xesamTitle = map[QStringLiteral("xesam:title")].toString();
                m_xesamUrl = map[QStringLiteral("xesam:url")].toString();
                m_xesamArtist = map[QStringLiteral("xesam:artist")].toStringList();
                m_xesamAlbumArtist = map[QStringLiteral("xesam:albumArtist")].toStringList();
                m_xesamAlbum = map[QStringLiteral("xesam:album")].toString();
                m_artUrl = map[QStringLiteral("mpris:artUrl")].toString();
                m_length = map[QStringLiteral("mpris:length")].toDouble();
                m_kdePid = map[QStringLiteral("kde:pid")].toUInt();
            }
            // we give out CanControl, as this may completely
            // change the UI of the widget
            else if (propName == QLatin1String("CanControl")) {
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
            // assume the position has reset to 0, since this is really the
            // only sensible value for a stopped track
            updatePosition();
        }
        Q_EMIT stateChanged();
        
    }
    void initBindings()
    {
        // Since the bindings are already used in QML, move them to C++ for better efficiency and consistency
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

        // Fake canStop property
        m_canStop.setBinding([this] {
            return m_canControl.value() && m_playbackStatus.value() > PlaybackStatus::Stopped;
        });

        // Metadata
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
            // if we play a local file without title and artist, show its containing folder instead
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

    /**
     * @brief Refresh player state by fetching all properties
     */
    void refresh()
    {
        // despite these calls being async, we should never update values in the
        // wrong order (eg: a stale GetAll response overwriting a more recent value
        // from a PropertiesChanged signal) due to D-Bus message ordering guarantees.
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

    void updatePosition()
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

    int m_fetchesPending = 0;
    QString m_busName;        ///< D-Bus service name
    OrgFreedesktopDBusPropertiesInterface *m_propsIface = nullptr;
    OrgMprisMediaPlayer2PlayerInterface *m_playerIface = nullptr;
    OrgMprisMediaPlayer2Interface *m_rootIface = nullptr;


    QPropertyNotifier m_rateNotifier;
    QPropertyNotifier m_playbackStatusNotifier;
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
            if(m_activePlayer) m_activePlayer->seek(pos);
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
        PlaybackStatus::Status status = container->playbackStatus();
        
        if (status == PlaybackStatus::Playing) {
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
                if (c->playbackStatus() == PlaybackStatus::Playing) {
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
        } else if (!m_activePlayer && status != PlaybackStatus::Unknown) {
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
        
        QVariantMap state;
        
        // Convert PlaybackStatus enum to string
        QString playbackStatusStr;
        switch (container->playbackStatus()) {
            case PlaybackStatus::Playing: playbackStatusStr = "Playing"; break;
            case PlaybackStatus::Paused: playbackStatusStr = "Paused"; break;
            case PlaybackStatus::Stopped: playbackStatusStr = "Stopped"; break;
            default: playbackStatusStr = "Unknown"; break;
        }
        
        state["state"] = playbackStatusStr;
        state["volume"] = container->volume();
        state["name"] = container->dbusname().replace("org.mpris.MediaPlayer2.","");
        state["title"] = container->track();
        state["artist"] = container->artist();
        state["album"] = container->album();
        state["art"] = container->artUrl();
        state["position"] = static_cast<qint64>(container->position()/1000000); // Convert to seconds
        state["duration"] = static_cast<qint64>(container->length()/1000000.0); // Already in seconds

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
    
private Q_SLOTS:
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