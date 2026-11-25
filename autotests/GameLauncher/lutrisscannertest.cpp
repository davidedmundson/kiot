// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

// How the Lutris scanner finds a game's Lutris id: the exe from the game's .yml against the
// paths in Lutris' game-paths.json. The id goes into `lutris lutris:rungameid/<id>`, so a wrong
// match launches another game.

#include <QJsonObject>
#include <QTest>

#include "LutrisScanner.h"

class LutrisScannerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testGameIdForExe_data()
    {
        QTest::addColumn<QString>("exe");
        QTest::addColumn<QString>("id");

        QTest::newRow("absolute") << QStringLiteral("/home/u/Games/witcher/drive_c/witcher3.exe") << QStringLiteral("7");
        QTest::newRow("absolute, unclean") << QStringLiteral("/home/u/Games/witcher//drive_c/./witcher3.exe") << QStringLiteral("7");
        QTest::newRow("relative") << QStringLiteral("drive_c/witcher3.exe") << QStringLiteral("7");
        QTest::newRow("relative file name") << QStringLiteral("game.exe") << QStringLiteral("3");
        QTest::newRow("no exe") << QString() << QString();
        QTest::newRow("blank exe") << QStringLiteral("  ") << QString();
        QTest::newRow("only a suffix of a file name") << QStringLiteral("cher3.exe") << QString();
        QTest::newRow("unknown") << QStringLiteral("/opt/other/game") << QString();
    }

    void testGameIdForExe()
    {
        QFETCH(QString, exe);
        QFETCH(QString, id);
        // "othergame.exe" ends with "game.exe" but is another file
        const QJsonObject gamePaths{{QStringLiteral("1"), QStringLiteral("/home/u/Games/other/othergame.exe")},
                                    {QStringLiteral("3"), QStringLiteral("/home/u/Games/mine/game.exe")},
                                    {QStringLiteral("5"), QString()},
                                    {QStringLiteral("7"), QStringLiteral("/home/u/Games/witcher/drive_c/witcher3.exe")}};
        QCOMPARE(LutrisScanner::gameIdForExe(gamePaths, exe), id);
    }
};

QTEST_GUILESS_MAIN(LutrisScannerTest)

#include "lutrisscannertest.moc"
