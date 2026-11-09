// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later


#include "notificationplugin.h"
#include <KNotification>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QHash>
#include <QJsonObject>
#include <QUrl>


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


namespace {
void sendNotification(const QString &title, const QString &body, const QJsonObject &options = {})
{
    static const QHash<QString, KNotification::Urgency> urgencies = {
        {QStringLiteral("low"), KNotification::LowUrgency},
        {QStringLiteral("normal"), KNotification::NormalUrgency},
        {QStringLiteral("high"), KNotification::HighUrgency},
        {QStringLiteral("critical"), KNotification::CriticalUrgency},
    };
    const auto urgency = urgencies.value(options.value(QStringLiteral("urgency")).toString(), KNotification::DefaultUrgency);
    const auto flags = urgency == KNotification::CriticalUrgency ? KNotification::Persistent : KNotification::CloseOnTimeout;

    // event() sends on the next event loop iteration, so the urgency can still be set
    auto notification = KNotification::event(QStringLiteral("notification"), title, body, QString(), flags);
    notification->setUrgency(urgency);

    const QUrl url(options.value(QStringLiteral("url")).toString());
    if (url.isValid() && !url.isEmpty()) {
        auto action = notification->addDefaultAction(QStringLiteral("Open"));
        QObject::connect(action, &KNotificationAction::activated, [url]() {
            QDesktopServices::openUrl(url);
        });
        notification->setHint(QStringLiteral("resident"), true);
    }
}
} // namespace

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
            sendNotification(title, body, obj);
 
        } else {
            // Plain text path, should never happen but better safe than sorry
            QString body = QString::fromUtf8(message);
            sendNotification(title, body);

        }

    }
#include "notificationplugin.moc"