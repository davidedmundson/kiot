// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <QFileInfo>
#include <QProcess>
#include <QTest>

#include "platformhelper.h"
#include "testenv.h"

using KIOTShared::PlatformHelper;

static KiotTest::IsolatedEnvironment s_env;

class PlatformHelperTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testNotFlatpak()
    {
        // testenv.h clears the variables; every test below that builds a command line relies on it
        QVERIFY(!PlatformHelper::isFlatpak());
    }

    void testIdentity()
    {
        QCOMPARE(PlatformHelper::getProjectName(), QStringLiteral(PROJECT_NAME));
        QCOMPARE(PlatformHelper::generateServiceName(), QStringLiteral(APP_ID));
        QCOMPARE(PlatformHelper::generateServiceName(), QStringLiteral("org.davidedmundson.kiot"));
    }

    void testConfigPaths()
    {
        const QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
        QCOMPARE(PlatformHelper::configDirPath(), configHome + QStringLiteral("/kiot"));

        const QString rc = PlatformHelper::configFilePath();
        QCOMPARE(rc, configHome + QStringLiteral("/kiotrc"));
        QVERIFY2(QFileInfo::exists(rc), "configFilePath() creates the file so KConfig can watch it");

        QCOMPARE(PlatformHelper::configFilePath(QStringLiteral(".json")), configHome + QStringLiteral("/kiot.json"));
    }

    void testResolveOrganizationDomain_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("empty") << QString() << QStringLiteral("davidedmundson.org");
        QTest::newRow("blank") << QStringLiteral("   ") << QStringLiteral("davidedmundson.org");
        QTest::newRow("homepage url") << QStringLiteral("https://github.com/davidedmundson/kiot") << QStringLiteral("github.com");
        QTest::newRow("bare host") << QStringLiteral("Example.COM") << QStringLiteral("example.com");
        QTest::newRow("bare host with path") << QStringLiteral("kde.org/kiot") << QStringLiteral("kde.org");
        QTest::newRow("http with port") << QStringLiteral("http://Home.Lan:8123/x") << QStringLiteral("home.lan");
    }

    void testResolveOrganizationDomain()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(PlatformHelper::resolveOrganizationDomain(input), expected);
    }

    void testDetectDesktopEnvironment_data()
    {
        QTest::addColumn<QString>("currentDesktop");
        QTest::addColumn<QString>("desktopSession");
        QTest::addColumn<QString>("expected");
        QTest::newRow("kde") << QStringLiteral("KDE") << QString() << QStringLiteral("kde");
        QTest::newRow("plasma session only") << QString() << QStringLiteral("plasmawayland") << QStringLiteral("kde");
        QTest::newRow("ubuntu gnome") << QStringLiteral("ubuntu:GNOME") << QString() << QStringLiteral("gnome");
        QTest::newRow("cosmic before pop") << QStringLiteral("pop:COSMIC") << QString() << QStringLiteral("cosmic");
        QTest::newRow("budgie") << QStringLiteral("Budgie:GNOME") << QString() << QStringLiteral("gnome");
        QTest::newRow("hyprland") << QStringLiteral("Hyprland") << QString() << QStringLiteral("hyprland");
        QTest::newRow("sway") << QStringLiteral("sway") << QString() << QStringLiteral("sway");
        QTest::newRow("xfce") << QStringLiteral("XFCE") << QString() << QStringLiteral("xfce");
        QTest::newRow("lxqt") << QStringLiteral("LXQt") << QString() << QStringLiteral("lxqt");
        QTest::newRow("cinnamon") << QStringLiteral("X-Cinnamon") << QString() << QStringLiteral("cinnamon");
        QTest::newRow("mate") << QStringLiteral("MATE") << QString() << QStringLiteral("mate");
        QTest::newRow("deepin") << QStringLiteral("Deepin") << QString() << QStringLiteral("dde");
        QTest::newRow("unknown with colon") << QStringLiteral("X-Foo:Bar") << QString() << QStringLiteral("x-foo");
        QTest::newRow("unknown") << QStringLiteral("niri") << QString() << QStringLiteral("niri");
        QTest::newRow("nothing set") << QString() << QString() << QStringLiteral("unknown");
    }

    void testDetectDesktopEnvironment()
    {
        QFETCH(QString, currentDesktop);
        QFETCH(QString, desktopSession);
        QFETCH(QString, expected);

        const auto set = [](const char *name, const QString &value) {
            if (value.isEmpty()) {
                qunsetenv(name);
            } else {
                qputenv(name, value.toUtf8());
            }
        };
        set("XDG_CURRENT_DESKTOP", currentDesktop);
        set("DESKTOP_SESSION", desktopSession);
        qunsetenv("XDG_SESSION_DESKTOP");

        QCOMPARE(PlatformHelper::detectDesktopEnvironment(), expected);
    }

    void testHostCommandLineOutsideFlatpak()
    {
        const QStringList args{QStringLiteral("--user"), QStringLiteral("start"), QStringLiteral("a b.service")};
        QCOMPARE(PlatformHelper::makeHostContext(QStringLiteral("systemctl"), args),
                 QStringList({QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("start"), QStringLiteral("a b.service")}));

        QProcess process;
        process.setProgram(QStringLiteral("systemctl"));
        process.setArguments(args);
        const auto context = PlatformHelper::makeHostContext(process);
        QCOMPARE(context.program, QStringLiteral("systemctl"));
        QCOMPARE(context.arguments, args);
    }
};

QTEST_GUILESS_MAIN(PlatformHelperTest)

#include "platformhelpertest.moc"
