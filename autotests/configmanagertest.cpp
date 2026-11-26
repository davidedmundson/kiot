// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

#include "Config/configmanager.h"
#include "platformhelper.h"
#include "testenv.h"

using KIOTShared::PlatformHelper;
using KIOTShared::Config::ConfigManager;

static KiotTest::IsolatedEnvironment s_env;

namespace
{
QString pluginsDir()
{
    return PlatformHelper::configDirPath() + QStringLiteral("/plugins");
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

void writeJson(const QString &path, const QJsonObject &object)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(object).toJson());
}
} // namespace

class ConfigManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cleanup()
    {
        QDir(pluginsDir()).removeRecursively();
    }

    void testCoreInstanceIsUnique()
    {
        ConfigManager core;
        QCOMPARE(core.filePath(), PlatformHelper::configDirPath() + QStringLiteral("/kiot"));

        // a second core config is demoted to a plugin config instead of sharing the core file
        ConfigManager second;
        QVERIFY(second.filePath() != core.filePath());
        QVERIFY(second.filePath().startsWith(pluginsDir() + QLatin1Char('/')));
    }

    void testPluginFileNames_data()
    {
        QTest::addColumn<QString>("moduleName");
        QTest::addColumn<QString>("fileName");
        QTest::newRow("lowercased") << QStringLiteral("Audio") << QStringLiteral("audio");
        QTest::newRow("trimmed") << QStringLiteral("  Mpris ") << QStringLiteral("mpris");
        QTest::newRow("path traversal") << QStringLiteral("../../.ssh/authorized_keys") << QStringLiteral(".sshauthorized_keys");
        QTest::newRow("backslashes") << QStringLiteral("..\\..\\x") << QStringLiteral("x");
        QTest::newRow("core is reserved") << QStringLiteral("Core") << QStringLiteral("invalid_plugin_name");
        QTest::newRow("empty") << QString() << QStringLiteral("unnamed_plugin");
        QTest::newRow("only dots") << QStringLiteral("..") << QStringLiteral("unnamed_plugin");
    }

    void testPluginFileNames()
    {
        QFETCH(QString, moduleName);
        QFETCH(QString, fileName);
        ConfigManager config(moduleName);
        QCOMPARE(config.filePath(), pluginsDir() + QLatin1Char('/') + fileName);
    }

    void testNestedValues()
    {
        ConfigManager config(QStringLiteral("nested"));
        QVERIFY(!config.contains(QStringLiteral("mqtt/host")));
        QCOMPARE(config.value(QStringLiteral("mqtt/host"), QStringLiteral("fallback")).toString(), QStringLiteral("fallback"));

        config.setValue(QStringLiteral("mqtt/host"), QStringLiteral("broker.lan"));
        config.setValue(QStringLiteral("mqtt/port"), 1884);
        config.setValue(QStringLiteral("top"), true);

        QVERIFY(config.contains(QStringLiteral("mqtt/host")));
        QCOMPARE(config.value(QStringLiteral("mqtt/host")).toString(), QStringLiteral("broker.lan"));
        QCOMPARE(config.value(QStringLiteral("mqtt/port")).toInt(), 1884);
        QCOMPARE(config.value(QStringLiteral("mqtt")).toObject().size(), 2);

        // a path through a value that is not an object finds nothing rather than crashing
        QVERIFY(!config.contains(QStringLiteral("top/deeper")));

        const QJsonObject onDisk = readJson(config.filePath());
        QCOMPARE(onDisk.value(QStringLiteral("mqtt")).toObject().value(QStringLiteral("host")).toString(), QStringLiteral("broker.lan"));
        QCOMPARE(onDisk.value(QStringLiteral("top")).toBool(), true);

        // and a new instance reads it back
        ConfigManager reread(QStringLiteral("nested"));
        QCOMPARE(reread.value(QStringLiteral("mqtt/port")).toInt(), 1884);
    }

    void testSetValueSignals()
    {
        ConfigManager config(QStringLiteral("signals"));
        QSignalSpy changed(&config, &ConfigManager::configChanged);
        config.setValue(QStringLiteral("a/b"), 3);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().at(0).toString(), QStringLiteral("a/b"));
        QCOMPARE(changed.first().at(1).value<QJsonValue>().toInt(), 3);
    }

    void testDefaultsMergeWithoutOverwriting()
    {
        QDir().mkpath(pluginsDir());
        writeJson(pluginsDir() + QStringLiteral("/defaults"),
                  QJsonObject{{QStringLiteral("nested"), QJsonObject{{QStringLiteral("x"), 5}}}, {QStringLiteral("own"), QStringLiteral("kept")}});

        const QJsonObject defaults{{QStringLiteral("a"), 1},
                                   {QStringLiteral("nested"), QJsonObject{{QStringLiteral("x"), 1}, {QStringLiteral("y"), 2}}}};
        ConfigManager config(QStringLiteral("defaults"), defaults);

        QCOMPARE(config.value(QStringLiteral("nested/x")).toInt(), 5);
        QCOMPARE(config.value(QStringLiteral("nested/y")).toInt(), 2);
        QCOMPARE(config.value(QStringLiteral("a")).toInt(), 1);
        QCOMPARE(config.value(QStringLiteral("own")).toString(), QStringLiteral("kept"));

        // the missing defaults are written to the file, the user's values stay
        const QJsonObject onDisk = readJson(config.filePath());
        QCOMPARE(onDisk.value(QStringLiteral("a")).toInt(), 1);
        QCOMPARE(onDisk.value(QStringLiteral("nested")).toObject().value(QStringLiteral("x")).toInt(), 5);
        QCOMPARE(onDisk.value(QStringLiteral("nested")).toObject().value(QStringLiteral("y")).toInt(), 2);

        config.setValue(QStringLiteral("a"), 9);
        config.resetToDefaults();
        QCOMPARE(config.value(QStringLiteral("a")).toInt(), 1);
        QCOMPARE(config.value(QStringLiteral("nested/x")).toInt(), 1);
        QVERIFY(!config.contains(QStringLiteral("own")));
        QCOMPARE(readJson(config.filePath()), defaults);
    }

    void testBrokenFileKeepsDefaults()
    {
        QDir().mkpath(pluginsDir());
        QFile file(pluginsDir() + QStringLiteral("/broken"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ not json");
        file.close();

        ConfigManager config(QStringLiteral("broken"), QJsonObject{{QStringLiteral("a"), 1}});
        QCOMPARE(config.value(QStringLiteral("a")).toInt(), 1);
    }

    void testRemove()
    {
        ConfigManager config(QStringLiteral("remove"));
        config.setValue(QStringLiteral("flat"), 1);
        config.setValue(QStringLiteral("group/key"), 2);

        config.remove(QStringLiteral("flat"));
        QVERIFY(!config.contains(QStringLiteral("flat")));
        QVERIFY(!readJson(config.filePath()).contains(QStringLiteral("flat")));

        config.remove(QStringLiteral("group/key"));
        // remove() takes the path as one top-level key, unlike value(), setValue() and contains()
        QEXPECT_FAIL("", "ConfigManager::remove() ignores '/' in the key path", Continue);
        QVERIFY(!config.contains(QStringLiteral("group/key")));
    }

    void testExternalEditIsReloaded()
    {
        ConfigManager config(QStringLiteral("watched"), QJsonObject{{QStringLiteral("a"), 1}});
        QSignalSpy changed(&config, &ConfigManager::configChanged);

        writeJson(config.filePath(), QJsonObject{{QStringLiteral("a"), 2}});

        QTRY_COMPARE(config.value(QStringLiteral("a")).toInt(), 2);
        QVERIFY(!changed.isEmpty());
        QCOMPARE(changed.last().at(0).toString(), QStringLiteral("*"));
    }

    void testValidateJsonString_data()
    {
        QTest::addColumn<QString>("json");
        QTest::addColumn<bool>("valid");
        QTest::newRow("object") << QStringLiteral(R"({"a": {"b": [1, 2]}})") << true;
        QTest::newRow("empty object") << QStringLiteral("{}") << true;
        QTest::newRow("array") << QStringLiteral("[1, 2]") << false;
        QTest::newRow("garbage") << QStringLiteral("{ a: 1") << false;
        QTest::newRow("empty") << QString() << false;
    }

    void testValidateJsonString()
    {
        QFETCH(QString, json);
        QFETCH(bool, valid);
        const auto result = ConfigManager::validateJsonString(json);
        QCOMPARE(result.success, valid);
        if (valid) {
            QCOMPARE(result.data, QJsonDocument::fromJson(json.toUtf8()).object());
        } else {
            QVERIFY(result.data.isEmpty());
        }
    }
};

QTEST_GUILESS_MAIN(ConfigManagerTest)

#include "configmanagertest.moc"
