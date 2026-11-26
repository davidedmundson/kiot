// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <KConfigGroup>
#include <KSharedConfig>
#include <QByteArray>
#include <QDir>
#include <QTemporaryDir>
#include <QVariantMap>

#include "platformhelper.h"

namespace KiotTest
{

/**
 * Gives the test process its own XDG config/data/cache/state directories and a D-Bus session
 * address that leads nowhere, before main() runs: declare one at file scope in each test
 * (`static KiotTest::IsolatedEnvironment s_env;`). A test then never touches the developer's own
 * ~/.config/kiotrc, and nothing it does reaches their desktop session over D-Bus.
 */
class IsolatedEnvironment
{
public:
    IsolatedEnvironment()
    {
        const QString root = m_dir.path();
        for (const char *name : {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"}) {
            const QString path = root + QLatin1Char('/') + QString::fromLatin1(name).toLower();
            QDir().mkpath(path);
            qputenv(name, QFile::encodeName(path));
        }
        qputenv("DBUS_SESSION_BUS_ADDRESS", QByteArray("unix:path=") + QFile::encodeName(root) + "/no-session-bus");
        qunsetenv("FLATPAK_ID");
        qunsetenv("FLATPAK_SANDBOX_DIR");
        qunsetenv("container");
    }

    QString path() const
    {
        return m_dir.path();
    }

private:
    QTemporaryDir m_dir;
};

/** Replaces the [general] group of kiotrc with @p entries. */
inline void writeGeneralConfig(const QVariantMap &entries)
{
    auto config = KSharedConfig::openConfig(KIOTShared::PlatformHelper::configFilePath(), KConfig::SimpleConfig);
    config->deleteGroup(QStringLiteral("general"));
    KConfigGroup group = config->group(QStringLiteral("general"));
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        group.writeEntry(it.key().toUtf8().constData(), it.value());
    }
    config->sync();
}

/** The [general] group of kiotrc as it is on disk now. */
inline KConfigGroup generalConfigOnDisk()
{
    auto config = KSharedConfig::openConfig(KIOTShared::PlatformHelper::configFilePath(), KConfig::SimpleConfig);
    config->reparseConfiguration();
    return config->group(QStringLiteral("general"));
}

} // namespace KiotTest
