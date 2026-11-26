// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

// The title and album the MPRIS media player entity shows for a player whose metadata has only xesam:url.

#include <QTest>

#include "metadatafallback.h"

class MetadataFallbackTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testTrack_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<QString>("track");

        QTest::newRow("local file") << QStringLiteral("file:///home/user/Music/Artist/Album/01%20Song.flac") << QStringLiteral("01 Song.flac");
        QTest::newRow("stream") << QStringLiteral("https://radio.example.org/live/stream.mp3") << QStringLiteral("stream.mp3");
        QTest::newRow("utf-8 percent-encoded") << QStringLiteral("file:///music/Caf%C3%A9.ogg") << QStringLiteral("Café.ogg");
        QTest::newRow("empty") << QString() << QString();
        QTest::newRow("trailing slash") << QStringLiteral("https://radio.example.org/live/") << QString();
        QTest::newRow("no slash") << QStringLiteral("song.flac") << QString();
    }

    void testTrack()
    {
        QFETCH(QString, url);
        QFETCH(QString, track);
        QCOMPARE(trackFromXesamUrl(url), track);
    }

    void testAlbum_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<QString>("album");

        QTest::newRow("local file") << QStringLiteral("file:///home/user/Music/Artist/Best%20Of/01%20Song.flac") << QStringLiteral("Best Of");
        QTest::newRow("file in the root") << QStringLiteral("file:///song.flac") << QString();
        QTest::newRow("not a local file") << QStringLiteral("https://example.org/Album/song.mp3") << QString();
        QTest::newRow("empty") << QString() << QString();
    }

    void testAlbum()
    {
        QFETCH(QString, url);
        QFETCH(QString, album);
        QCOMPARE(albumFromXesamUrl(url), album);
    }
};

QTEST_GUILESS_MAIN(MetadataFallbackTest)
#include "metadatafallbacktest.moc"
