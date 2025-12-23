#pragma once

#include <QBindable>
#include <QObject>

#include <QtQml/qqmlregistration.h>

class QDBusPendingCallWatcher;

class OrgFreedesktopDBusPropertiesInterface;
class OrgMprisMediaPlayer2Interface;
class OrgMprisMediaPlayer2PlayerInterface;

inline constexpr QLatin1String MPRIS2_PATH{"/org/mpris/MediaPlayer2"};
inline constexpr QLatin1String MPRIS2_PREFIX{"org.mpris.MediaPlayer2."};

// Remove KMPRIS_EXPORT since we're not building a library
#ifndef KMPRIS_EXPORT
#define KMPRIS_EXPORT
#endif

namespace LoopStatus
{
QML_ELEMENT
KMPRIS_EXPORT Q_NAMESPACE enum KMPRIS_EXPORT Status : std::uint32_t {
    Unknown = 0,
    None,
    Playlist,
    Track,
};
KMPRIS_EXPORT Q_ENUM_NS(Status)
}

namespace ShuffleStatus
{
QML_ELEMENT
KMPRIS_EXPORT Q_NAMESPACE enum KMPRIS_EXPORT Status : std::uint32_t {
    Unknown = 0,
    Off,
    On,
};
KMPRIS_EXPORT Q_ENUM_NS(Status)
}

namespace PlaybackStatus
{
QML_ELEMENT
KMPRIS_EXPORT Q_NAMESPACE enum KMPRIS_EXPORT Status : std::uint32_t {
    Unknown = 0,
    Stopped,
    Playing,
    Paused,
};
KMPRIS_EXPORT Q_ENUM_NS(Status)
}

/**
 * A class with empty player information
 */
class KMPRIS_EXPORT AbstractPlayerContainer : public QObject
{
    Q_OBJECT
    // From org.mpris.MediaPlayer2.Player
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY canGoNextChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY canGoPreviousChanged)
    Q_PROPERTY(bool canPlay READ canPlay NOTIFY canPlayChanged)
    Q_PROPERTY(bool canPause READ canPause NOTIFY canPauseChanged)
    Q_PROPERTY(bool canStop READ canStop NOTIFY canStopChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY canSeekChanged)
    Q_PROPERTY(bool canControl READ canControl NOTIFY canControlChanged)
    Q_PROPERTY(LoopStatus::Status loopStatus READ loopStatus NOTIFY loopStatusChanged)
    Q_PROPERTY(double maximumRate READ maximumRate NOTIFY maximumRateChanged)
    Q_PROPERTY(double minimumRate READ minimumRate NOTIFY minimumRateChanged)
    Q_PROPERTY(PlaybackStatus::Status playbackStatus READ playbackStatus NOTIFY playbackStatusChanged)
    Q_PROPERTY(qlonglong position READ position NOTIFY positionChanged)
    Q_PROPERTY(double rate READ rate NOTIFY rateChanged)
    Q_PROPERTY(ShuffleStatus::Status shuffle READ shuffle NOTIFY shuffleChanged)
    Q_PROPERTY(double volume READ volume NOTIFY volumeChanged)

    // From metadata map
    Q_PROPERTY(QString track READ track NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY artistChanged)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY artUrlChanged)
    Q_PROPERTY(QString album READ album NOTIFY albumChanged)
    Q_PROPERTY(double length READ length NOTIFY lengthChanged)

    /**
     * Used to filter p-b-i player
     */
    Q_PROPERTY(unsigned kdePid READ kdePid NOTIFY kdePidChanged)

    // From org.mpris.MediaPlayer2
    Q_PROPERTY(bool canQuit READ canQuit NOTIFY canQuitChanged)
    Q_PROPERTY(bool canRaise READ canRaise NOTIFY canRaiseChanged)
    Q_PROPERTY(bool canSetFullscreen READ canSetFullscreen NOTIFY canSetFullscreenChanged)
    Q_PROPERTY(QStringList supportedMimeTypes READ supportedMimeTypes NOTIFY supportedMimeTypesChanged)
    Q_PROPERTY(QStringList supportedUriSchemes READ supportedUriSchemes NOTIFY supportedUriSchemesChanged)
    Q_PROPERTY(QString desktopEntry READ desktopEntry NOTIFY desktopEntryChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY desktopEntryChanged)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool hasTrackList READ hasTrackList NOTIFY hasTrackListChanged)
    Q_PROPERTY(QString identity READ identity NOTIFY identityChanged)
    Q_PROPERTY(unsigned instancePid READ instancePid CONSTANT)

public:
    explicit AbstractPlayerContainer(QObject *parent = nullptr);
    ~AbstractPlayerContainer() override;

    bool canControl() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canPause() const;
    bool canPlay() const;
    bool canStop() const;
    bool canSeek() const;

    LoopStatus::Status loopStatus() const;

    double maximumRate() const;
    double minimumRate() const;
    PlaybackStatus::Status playbackStatus() const;

    qlonglong position() const;

    double rate() const;

    ShuffleStatus::Status shuffle() const;

    double volume() const;

    QString track() const;
    QString artist() const;
    QString artUrl() const;
    QString album() const;
    double length() const;

    unsigned instancePid() const;
    unsigned kdePid() const;

    bool canQuit() const;
    bool canRaise() const;
    bool canSetFullscreen() const;
    QString desktopEntry() const;
    bool fullscreen() const;
    bool hasTrackList() const;
    QString identity() const;
    QStringList supportedMimeTypes() const;
    QStringList supportedUriSchemes() const;
    QString iconName() const;

Q_SIGNALS:
    void canGoNextChanged();
    void canGoPreviousChanged();
    void canPlayChanged();
    void canPauseChanged();
    void canStopChanged();
    void canSeekChanged();
    void canControlChanged();
    void loopStatusChanged();
    void maximumRateChanged();
    void metadataChanged();
    void minimumRateChanged();
    void playbackStatusChanged();
    void positionChanged();
    void rateChanged();
    void shuffleChanged();
    void volumeChanged();

    void trackChanged();
    void artistChanged();
    void artUrlChanged();
    void albumChanged();
    void lengthChanged();
    void kdePidChanged();

    void canQuitChanged();
    void canSetFullscreenChanged();
    void canRaiseChanged();
    void supportedMimeTypesChanged();
    void supportedUriSchemesChanged();
    void desktopEntryChanged();
    void fullscreenChanged();
    void hasTrackListChanged();
    void identityChanged();

protected:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_canControl, false, &AbstractPlayerContainer::canControlChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_canGoNext, false)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_effectiveCanGoNext, false, &AbstractPlayerContainer::canGoNextChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_canGoPrevious, false)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_effectiveCanGoPrevious, false, &AbstractPlayerContainer::canGoPreviousChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_canPlay, false)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_effectiveCanPlay, false, &AbstractPlayerContainer::canPlayChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_canPause, false)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_effectiveCanPause, false, &AbstractPlayerContainer::canPauseChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_canStop, false)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_effectiveCanStop, false, &AbstractPlayerContainer::canStopChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_canSeek, false, &AbstractPlayerContainer::canSeekChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_effectiveCanSeek, false, &AbstractPlayerContainer::canSeekChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer,
                                         LoopStatus::Status,
                                         m_loopStatus,
                                         LoopStatus::Unknown,
                                         &AbstractPlayerContainer::loopStatusChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer,
                                         ShuffleStatus::Status,
                                         m_shuffle,
                                         ShuffleStatus::Unknown,
                                         &AbstractPlayerContainer::shuffleChanged)

    // From metadata
    Q_OBJECT_BINDABLE_PROPERTY(AbstractPlayerContainer, QString, m_trackId)
    Q_OBJECT_BINDABLE_PROPERTY(AbstractPlayerContainer, QString, m_xesamTitle)
    Q_OBJECT_BINDABLE_PROPERTY(AbstractPlayerContainer, QString, m_xesamUrl)
    Q_OBJECT_BINDABLE_PROPERTY(AbstractPlayerContainer, QString, m_xesamAlbum)
    Q_OBJECT_BINDABLE_PROPERTY(AbstractPlayerContainer, QStringList, m_xesamArtist)
    Q_OBJECT_BINDABLE_PROPERTY(AbstractPlayerContainer, QStringList, m_xesamAlbumArtist)
    Q_OBJECT_BINDABLE_PROPERTY(AbstractPlayerContainer, QString, m_track, &AbstractPlayerContainer::trackChanged)
    Q_OBJECT_BINDABLE_PROPERTY(AbstractPlayerContainer, QString, m_artist, &AbstractPlayerContainer::artistChanged)
    Q_OBJECT_BINDABLE_PROPERTY(AbstractPlayerContainer, QString, m_artUrl, &AbstractPlayerContainer::artUrlChanged)
    Q_OBJECT_BINDABLE_PROPERTY(AbstractPlayerContainer, QString, m_album, &AbstractPlayerContainer::albumChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, double, m_length, 0.0, &AbstractPlayerContainer::lengthChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, double, m_minimumRate, 1.0, &AbstractPlayerContainer::minimumRateChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, double, m_maximumRate, 1.0, &AbstractPlayerContainer::maximumRateChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer,
                                         PlaybackStatus::Status,
                                         m_playbackStatus,
                                         PlaybackStatus::Unknown,
                                         &AbstractPlayerContainer::playbackStatusChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, qlonglong, m_position, 0, &AbstractPlayerContainer::positionChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, double, m_rate, 1.0, &AbstractPlayerContainer::rateChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, double, m_volume, 0.0, &AbstractPlayerContainer::volumeChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_canQuit, false, &AbstractPlayerContainer::canQuitChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_canRaise, false, &AbstractPlayerContainer::canRaiseChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_canSetFullscreen, false, &AbstractPlayerContainer::canSetFullscreenChanged)
    QStringList m_supportedMimeTypes;
    QStringList m_supportedUriSchemes;
    QString m_desktopEntry;
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_fullscreen, false, &AbstractPlayerContainer::fullscreenChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, bool, m_hasTrackList, false, &AbstractPlayerContainer::hasTrackListChanged)
    QString m_identity;

    QString m_iconName = QStringLiteral("emblem-music-symbolic");
    unsigned m_instancePid = 0;
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractPlayerContainer, unsigned, m_kdePid, 0, &AbstractPlayerContainer::kdePidChanged)
};