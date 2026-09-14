// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/notify.h"
#include <KNotification>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
class Notifications : public QObject
{
    Q_OBJECT
public:
    explicit Notifications(QObject *parent)
        :   QObject(parent)
    {
        m_notify = new Notify(this);
        m_notify->setId("notifications");
        m_notify->setName("Notifications");
        connect(m_notify, &Notify::notificationReceived, this, &Notifications::notificationCallback);
    }

    void notificationCallback(QByteArray message)
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

private:
    Notify *m_notify;
};

void setupNotifications()
{
    new Notifications(qApp);
}

REGISTER_INTEGRATION("Notifications", setupNotifications, true)
#include "notifications.moc"
