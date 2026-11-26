// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

// The "url" of a notification arrives over MQTT; only web and mail links may be opened from it.

#include <QTest>

#include "notificationurl.h"

class NotificationUrlTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testUrl_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("openable");

        QTest::newRow("https") << QStringLiteral("https://homeassistant.local:8123/dashboard-home") << true;
        QTest::newRow("http") << QStringLiteral("http://192.168.1.10:8123/") << true;
        QTest::newRow("upper-case scheme") << QStringLiteral("HTTPS://example.org/") << true;
        QTest::newRow("mailto") << QStringLiteral("mailto:someone@example.org") << true;

        QTest::newRow("empty") << QString() << false;
        QTest::newRow("file") << QStringLiteral("file:///home/user/Downloads/evil.desktop") << false;
        QTest::newRow("relative path") << QStringLiteral("/home/user/script.sh") << false;
        QTest::newRow("custom scheme") << QStringLiteral("steam://run/440") << false;
        QTest::newRow("javascript") << QStringLiteral("javascript:alert(1)") << false;
        QTest::newRow("smb") << QStringLiteral("smb://server/share") << false;
        QTest::newRow("http without host") << QStringLiteral("http:foo") << false;
        QTest::newRow("mailto without address") << QStringLiteral("mailto:") << false;
        QTest::newRow("invalid") << QStringLiteral("http://exa mple.org/") << false;
    }

    void testUrl()
    {
        QFETCH(QString, url);
        QFETCH(bool, openable);
        QCOMPARE(isOpenableNotificationUrl(QUrl(url)), openable);
    }
};

QTEST_GUILESS_MAIN(NotificationUrlTest)
#include "notificationurltest.moc"
