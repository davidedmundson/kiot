// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file systemdplugin.cpp
 * @brief Implementation of the KIOT plugin systemdplugin.
 */

#include "systemdplugin.h"
#include <QString>
#include <QCoreApplication>

// Add the entities you need from the shared lib like this:
// using KIOTShared::Entities::Sensor;

using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(systemdlogger, SystemD)

SystemdPlugin::SystemdPlugin(QObject *parent)
    : QObject(parent)
{
}

SystemdPlugin::~SystemdPlugin()
{
    stopPlugin();
}

QString SystemdPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString SystemdPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl SystemdPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber SystemdPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool SystemdPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasSystemdTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.freedesktop.systemd1", "talk");
        if (!hasSystemdTalk) {
            qCWarning(systemdlogger) << "Missing required Flatpak D-Bus permission to talk to org.freedesktop.systemd1.";
            qCInfo(systemdlogger) << "To fix this, run:";
            qCInfo(systemdlogger) << "  flatpak --user override --talk-name=org.freedesktop.systemd1 " + appId;
            return false;
        }
    }

    return true;
}

bool SystemdPlugin::enabledByDefault()
{
    if (!checkCompatibility()) {
        return false;
    }
    return true; 
}
bool SystemdPlugin::startPlugin()
{
    if(m_watcher)
        stopPlugin();
    m_watcher = new SystemDWatcher(this);
    qCInfo(systemdlogger) << name() << "plugin started successfully";
    return true;
}

bool SystemdPlugin::stopPlugin()
{
    if(m_watcher)
    {
        m_watcher->deleteLater();
        m_watcher = nullptr;
    }
    qCInfo(systemdlogger) << name() << "plugin stopped";
    return true;
}

#include "systemdplugin.moc"