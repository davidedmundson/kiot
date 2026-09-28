// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later


#include "notificationplugin.h"
#include <KNotification>
#include <QCoreApplication>


using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger,Notifications)

NotificationPlugin::NotificationPlugin(QObject *parent)
    : QObject(parent)
{
}

QString NotificationPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString NotificationPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl NotificationPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber NotificationPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool NotificationPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasNotificationsTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.freedesktop.Notifications", "talk");
        if (!hasNotificationsTalk) {
            qCWarning(plugin_logger) << "Missing required Flatpak D-Bus permission for desktop notifications.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --talk-name=org.freedesktop.Notifications " + appId;
            return false;
        }
    }

    return true;
}
bool NotificationPlugin::enabledByDefault()
{
    if (!checkCompatibility()) {
        return false;
    }
    return true; 
}
bool NotificationPlugin::startPlugin()
{
    if(m_notify)
        stopPlugin();

    m_notify = new Notify(this);
    m_notify->setId("notifications");
    m_notify->setName("Notifications");
    connect(m_notify, &Notify::notificationReceived, this, &NotificationPlugin::notificationCallback);
    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool NotificationPlugin::stopPlugin()
{
    if(m_notify)
    {
        m_notify->deleteLater();
        m_notify = nullptr;
    }
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}


void NotificationPlugin::notificationCallback(QByteArray message)
    {
        QString title = QString(PROJECT_NAME);
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(message, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();

            
            //SO home assistant mqtt notify entity does not support title as default
            //but just incase of a hacker, we do a extra check to set it if included
            if(obj.contains("title"))
                title = obj.value("title").toString();
            const QString body = obj.value("message").toString();
            KNotification::event(KNotification::Notification, title, body);
 
        } else {
            // Plain text path, should never happen but better safe than sorry
            QString body = QString::fromUtf8(message);
            KNotification::event(KNotification::Notification, title, body);

        }

    }
#include "notificationplugin.moc"