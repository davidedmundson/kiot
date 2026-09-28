// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

// SPDX-FileCopyrightText: 1998 Sven Radej <sven@lisa.exp.univie.ac.at>
//      SPDX-FileCopyrightText: 2006 Dirk Mueller <mueller@kde.org>
//          SPDX-FileCopyrightText: 2007 Flavio Castelli <flavio.castelli@gmail.com>

#include "cameraplugin.h"
#include <QCoreApplication>

using KIOTShared::Entities::BinarySensor;
using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger,Camera)

CameraPlugin::CameraPlugin(QObject *parent)
    : QObject(parent)
{
}

QString CameraPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString CameraPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl CameraPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber CameraPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}


bool CameraPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        bool hasDeviceAccess = PlatformHelper::checkFlatpakFeature("Context", "devices", "all");
        if (!hasDeviceAccess) {
            qCWarning(plugin_logger) << "Missing required Flatpak device access to monitor webcams.";
            qCInfo(plugin_logger) << "To fix this, you can grant access using Flatseal or by running:";
            qCInfo(plugin_logger) << "  flatpak --user override --device=all " + PlatformHelper::generateServiceName(); 
            return false;
        }
    }
    return true;
}
bool CameraPlugin::enabledByDefault()
{
    if(!checkCompatibility())
        return false;

    return true;
}

bool CameraPlugin::startPlugin()
{
    if(!checkCompatibility())
    {
        qCWarning(plugin_logger) << name() << " plugin is not compatible with this system";
        return false;
    }
    if(m_cameraWatcher)
        stopPlugin();

    m_cameraWatcher = new CameraWatcher(this);
    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool CameraPlugin::stopPlugin()
{
    if(m_cameraWatcher)
    {
        m_cameraWatcher->deleteLater();
        m_cameraWatcher = nullptr;
    }
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}

#include "cameraplugin.moc"