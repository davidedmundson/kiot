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

#pragma once
#include <QObject>
#include <QPointer>

class PlayerContainer;
class QNetworkAccessManager;
class QNetworkReply;
namespace KIOTShared::Entities {
class MediaPlayer;
}

class MprisMultiplexer : public QObject
{
    Q_OBJECT
public:
    explicit MprisMultiplexer(QObject *parent = nullptr);
    ~MprisMultiplexer() override;

private Q_SLOTS:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    void setupMediaPlayer();
    void discoverPlayers();
    void addPlayer(const QString &busName);
    void removePlayer(const QString &busName);
    void handleActivePlayer(PlayerContainer *container);
    void downloadArt(const QString &url);
    void updateMediaPlayerEntity(PlayerContainer *container);

    QList<PlayerContainer *> m_containers;
    PlayerContainer *m_activePlayer = nullptr;
    KIOTShared::Entities::MediaPlayer *m_playerEntity = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_artReply; // the album art download in flight
};