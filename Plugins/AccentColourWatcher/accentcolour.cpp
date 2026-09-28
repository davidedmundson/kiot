// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "accentcolour.h"
#include <QCoreApplication>
#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>

using KIOTShared::Entities::Sensor;
using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger_accentcolour,AccentColourWatcher)



AccentColourWatcherPlugin::AccentColourWatcherPlugin(QObject *parent)
    : QObject(parent)
{
}

QString AccentColourWatcherPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString AccentColourWatcherPlugin::description() const
{
    return QStringLiteral(PLUGIN_DESCRIPTION).replace("\"", "") + QStringLiteral(" ").replace("\"", "") + QStringLiteral(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl AccentColourWatcherPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber AccentColourWatcherPlugin::version() const
{
    QString version = QStringLiteral(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool AccentColourWatcherPlugin::checkCompatibility()
{
    auto desktopEnviornment = PlatformHelper::detectDesktopEnvironment();
    if(desktopEnviornment == "kde")
    {
        if(PlatformHelper::isFlatpak())
        {
           auto hasAccess = PlatformHelper::checkFlatpakFeature("Context", "filesystems", "xdg-config/kdeglobals:ro");
            if (!hasAccess) {
                qCWarning(plugin_logger_accentcolour) << "Missing required Flatpak permission to access 'kdeglobals'.";
                qCInfo(plugin_logger_accentcolour) << "To fix this, you can grant read access using Flatseal or by running:";
                qCInfo(plugin_logger_accentcolour) << "  flatpak --user override --filesystem=xdg-config/kdeglobals:ro " + PlatformHelper::generateServiceName(); // (eller tilsvarende app-id)
                return false;
            }
        }
        return true;
    }
    
        qCWarning(plugin_logger_accentcolour) << "This plugin is only compatible with KDE Plasma";
    return false;
}

bool AccentColourWatcherPlugin::enabledByDefault()
{
    if(!checkCompatibility())
        return false;

    return true;
}
bool AccentColourWatcherPlugin::startPlugin()
{
    if(!checkCompatibility())
        return false;
    if(m_watcher)
        stopPlugin();
    m_watcher = new AccentColourWatcher(this);
    if(!m_watcher)
        return false;
    return true;
}

bool AccentColourWatcherPlugin::stopPlugin()
{
    if(m_watcher)
    {
        m_watcher->deleteLater();
        m_watcher = nullptr;
    }
    qCInfo(plugin_logger_accentcolour) << name() << " plugin stopped";
    return true;
}

#include "accentcolour.moc"