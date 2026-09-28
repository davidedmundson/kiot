// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "dnd.h"
#include <QCoreApplication>

using KIOTShared::Entities::BinarySensor;
using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger,DnD)

DnDPlugin::DnDPlugin(QObject *parent)
    : QObject(parent)
{
}

QString DnDPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString DnDPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl DnDPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber DnDPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool DnDPlugin::checkCompatibility()
{

    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasNotifiTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.freedesktop.Notifications", "talk");
        if (!hasNotifiTalk) {
            qCWarning(plugin_logger) << "Missing required Flatpak permission to talk to org.freedesktop.Notifications";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --talk-name=org.freedesktop.Notifications " + appId;
            return false;
        }
    }
    return true;
}

bool DnDPlugin::enabledByDefault()
{
    if(!checkCompatibility())
        return false;

    return true;
}

bool DnDPlugin::startPlugin()
{
    if(m_dndProperty || m_dndSensor)
        stopPlugin();
    m_dndSensor = new BinarySensor(this);
    m_dndSensor->setId("dnd");
    m_dndSensor->setName("Do not disturb");

    m_dndProperty = new DBusProperty("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Inhibited", this);
    QObject::connect(m_dndProperty, &DBusProperty::valueChanged, this, [this](const QVariant &value) {
        m_dndSensor->setState(value.toBool());
    });
    m_dndSensor->setState(m_dndProperty->value().toBool());
    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool DnDPlugin::stopPlugin()
{
    if(m_dndProperty)
    {
        disconnect(m_dndProperty,nullptr,this,nullptr);
        m_dndProperty->deleteLater();
        m_dndProperty = nullptr;
    }
    if(m_dndSensor)
    {
        m_dndSensor->deleteLater();
        m_dndSensor = nullptr;
    }
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}

#include "dnd.moc"