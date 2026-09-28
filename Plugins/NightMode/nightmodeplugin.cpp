// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "nightmodeplugin.h"

#include <QCoreApplication>

using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger,NightMode)

NightModePlugin::NightModePlugin(QObject *parent)
    : QObject(parent)
{
}

QString NightModePlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString NightModePlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl NightModePlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber NightModePlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool NightModePlugin::checkCompatibility()
{
    auto desktopEnvironment = PlatformHelper::detectDesktopEnvironment();
    if (desktopEnvironment != "kde") {
        return false;
    }

    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasKWinTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.kde.KWin", "talk");
        if (!hasKWinTalk) {
            qCWarning(plugin_logger) << "Missing required Flatpak permission to talk to org.kde.KWin.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --talk-name=org.kde.KWin " + appId;
            return false;
        }
    }
    return true;
}

bool NightModePlugin::enabledByDefault()
{
    if (!checkCompatibility()) {
        return false;
    }
    return true; 
}

bool NightModePlugin::startPlugin()
{
    if(m_nightMode)
        stopPlugin();

    m_nightMode = new NightMode(this);
    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool NightModePlugin::stopPlugin()
{
    if(m_nightMode)
    {
        m_nightMode->deleteLater();
        m_nightMode = nullptr;
    }
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}

#include "nightmodeplugin.moc"