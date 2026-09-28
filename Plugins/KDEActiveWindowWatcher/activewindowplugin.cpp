// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "activewindowplugin.h"
#include "kdeactivewindowwatcher.h"

using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger_activewindow, KDEActiveWindowWatcher)

ActiveWindowPlugin::ActiveWindowPlugin(QObject *parent)
    : QObject(parent)
{
}

ActiveWindowPlugin::~ActiveWindowPlugin()
{
    stopPlugin();
}

QString ActiveWindowPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString ActiveWindowPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}

QUrl ActiveWindowPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}

QVersionNumber ActiveWindowPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool ActiveWindowPlugin::checkCompatibility()
{
    auto desktopEnvironment = PlatformHelper::detectDesktopEnvironment();
    if (desktopEnvironment != "kde") {
        return false;
    }

    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasKWinTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.kde.KWin", "talk");
        if (!hasKWinTalk) {
            qCWarning(plugin_logger_activewindow) << "Missing required Flatpak permission to talk to org.kde.KWin.";
            qCInfo(plugin_logger_activewindow) << "To fix this, run:";
            qCInfo(plugin_logger_activewindow) << "  flatpak --user override --talk-name=org.kde.KWin " + appId;
            return false;
        }
    }

    return true;
}
bool ActiveWindowPlugin::enabledByDefault()
{
    if (!checkCompatibility()) {
        return false;
    }
    return true; 
}

bool ActiveWindowPlugin::startPlugin()
{
    if (!m_watcher) {
        m_watcher = new KDEActiveWindowWatcher(this);
    }
    qCInfo(plugin_logger_activewindow) << name() << "plugin started successfully";
    return true;
}

bool ActiveWindowPlugin::stopPlugin()
{
    if (m_watcher) {
        delete m_watcher;
        m_watcher = nullptr;
    }
    qCInfo(plugin_logger_activewindow) << name() << "plugin stopped";
    return true;
}

#include "activewindowplugin.moc"