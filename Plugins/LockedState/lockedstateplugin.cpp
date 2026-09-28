// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "lockedstateplugin.h"

using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger,LockedState)

LockedStatePlugin::LockedStatePlugin(QObject *parent)
    : QObject(parent)
{
}
LockedStatePlugin::~LockedStatePlugin()
{
}
QString LockedStatePlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString LockedStatePlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl LockedStatePlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber LockedStatePlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool LockedStatePlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        // Sjekk tilgang til ScreenSaver på sesjonsbussen
        bool hasScreenSaverTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.freedesktop.ScreenSaver", "talk");
        // Sjekk tilgang til login1 på systembussen
        bool hasLogin1Talk = PlatformHelper::checkFlatpakFeature("System Bus Policy", "org.freedesktop.login1", "talk");

        if (!hasScreenSaverTalk || !hasLogin1Talk) {
            qCWarning(plugin_logger) << "Missing required Flatpak D-Bus permissions for LockedState plugin.";
            
            if (!hasScreenSaverTalk) {
                qCInfo(plugin_logger) << "To fix ScreenSaver access, run:";
                qCInfo(plugin_logger) << "  flatpak --user override --talk-name=org.freedesktop.ScreenSaver " + appId;
            }
            if (!hasLogin1Talk) {
                qCInfo(plugin_logger) << "To fix systemd login1 access, run:";
                qCInfo(plugin_logger) << "  flatpak --user override --system-talk-name=org.freedesktop.login1 " + appId;
            }
            return false;
        }
    }

    return true;
}

bool LockedStatePlugin::enabledByDefault()
{
    if (!checkCompatibility()) {
        return false;
    }
    return true; 
}

bool LockedStatePlugin::startPlugin()
{
    if(m_lockedState)
        stopPlugin();
    m_lockedState = new LockedState(this);
    //m_lockedState = new LockedState(this);
    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool LockedStatePlugin::stopPlugin()
{
    if(m_lockedState)
    {
        m_lockedState->deleteLater();
        m_lockedState = nullptr;
    }

    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}

#include "lockedstateplugin.moc"