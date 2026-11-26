// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

// The SystemD plugin finds the switch for a unit's PropertiesChanged signal by turning the unit's
// D-Bus object path back into the unit name.

#include <QTest>

#include "systemd_user.h"

class SystemdUnitNameTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testPathToUnitName_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QString>("unit");

        QTest::newRow("dot and dash") << QStringLiteral("/org/freedesktop/systemd1/unit/pipewire_2dpulse_2eservice")
                                      << QStringLiteral("pipewire-pulse.service");
        QTest::newRow("underscore") << QStringLiteral("/org/freedesktop/systemd1/unit/my_5fagent_2eservice")
                                    << QStringLiteral("my_agent.service");
        QTest::newRow("template instance") << QStringLiteral("/org/freedesktop/systemd1/unit/syncthing_40alice_2eservice")
                                           << QStringLiteral("syncthing@alice.service");
        QTest::newRow("leading digit") << QStringLiteral("/org/freedesktop/systemd1/unit/_31password_2eservice")
                                       << QStringLiteral("1password.service");
        QTest::newRow("colon and backslash") << QStringLiteral("/org/freedesktop/systemd1/unit/a_3ab_5cx2dc_2eservice")
                                             << QStringLiteral("a:b\\x2dc.service");
        QTest::newRow("utf-8") << QStringLiteral("/org/freedesktop/systemd1/unit/caf_c3_a9_2eservice")
                               << QStringLiteral("café.service");
        QTest::newRow("invalid escape kept") << QStringLiteral("/org/freedesktop/systemd1/unit/a_zz_2") << QStringLiteral("a_zz_2");
        QTest::newRow("empty name") << QStringLiteral("/org/freedesktop/systemd1/unit/_") << QString();
    }

    void testPathToUnitName()
    {
        QFETCH(QString, path);
        QFETCH(QString, unit);
        QCOMPARE(SystemDWatcher::pathToUnitName(path), unit);
    }
};

QTEST_GUILESS_MAIN(SystemdUnitNameTest)

#include "systemdunitnametest.moc"
