// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "bluetoothplugin.h"

#include <QCoreApplication>

using KIOTShared::Entities::BinarySensor;
using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger,Bluetooth)

BluetoothPlugin::BluetoothPlugin(QObject *parent)
    : QObject(parent)
{
}

QString BluetoothPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString BluetoothPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl BluetoothPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber BluetoothPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool BluetoothPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasBluezTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.bluez", "talk");
        if (!hasBluezTalk) {
            qCWarning(plugin_logger) << "Missing required Flatpak permission to talk to BlueZ.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --talk-name=org.bluez" << appId;
            return false;
        }
        bool hasBluetoothSocket = PlatformHelper::checkFlatpakFeature("Context", "features", "bluetooth"); 
        if (!hasBluetoothSocket) {
            qCWarning(plugin_logger) << "Missing required Flatpak bluetooth socket permission (--allow=bluetooth).";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --allow=bluetooth" << appId;
            return false;
        }
    }
    return true;
}
bool BluetoothPlugin::enabledByDefault()
{
    if(!checkCompatibility())
        return false;

    return true;
}

bool BluetoothPlugin::startPlugin()
{
    if(m_adapterWatcher)
        stopPlugin();

    m_adapterWatcher = new BluetoothAdapterWatcher(this);
    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool BluetoothPlugin::stopPlugin()
{
    if(m_adapterWatcher)
    {
        m_adapterWatcher->deleteLater();
        m_adapterWatcher = nullptr;
    }
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}

#include "bluetoothplugin.moc"