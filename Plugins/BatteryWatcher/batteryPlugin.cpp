// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "batteryPlugin.h"



using KIOTShared::Entities::Sensor;
using KIOTShared::PlatformHelper;


DEFINE_PLUGIN_LOGGER(plugin_loggerbatter,BatteryWatcher)
BatteryPlugin::BatteryPlugin(QObject *parent)
    : QObject(parent)
{
}

QString BatteryPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString BatteryPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl BatteryPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber BatteryPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool BatteryPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();        
        bool hasUPower = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.freedesktop.UPower", "talk");
        if (!hasUPower) {
            qCWarning(plugin_loggerbatter) << "Missing required Flatpak permission to talk to UPower.";
            qCInfo(plugin_loggerbatter) << "To fix this, you can grant access using Flatseal or by running:";
            qCInfo(plugin_loggerbatter) << "  flatpak --user override --talk-name=org.freedesktop.UPower " + appId;
            return false;
        }
    }
    return true;
}
bool BatteryPlugin::enabledByDefault()
{
    if(!checkCompatibility())
        return false;

    return true;
}

bool BatteryPlugin::startPlugin()
{
    if(m_batteryWatcher)
        stopPlugin();
    m_batteryWatcher = new BatteryWatcher(this);

    qCInfo(plugin_loggerbatter) << name() << " plugin started successfully";
    return true;
}

bool BatteryPlugin::stopPlugin()
{
    if(m_batteryWatcher)
    {
        m_batteryWatcher->deleteLater();
        m_batteryWatcher = nullptr;
    }
    qCInfo(plugin_loggerbatter) << name() << " plugin stopped";
    return true;
}

#include "batteryPlugin.moc"