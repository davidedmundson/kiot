// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"

#include <QCoreApplication>
#include <QMqttClient>

#include <QJsonDocument>
#include <QJsonObject>

#include <KNotification>

class Notifications: public Entity
{
    Q_OBJECT
public:
    Q_INVOKABLE Notifications(QObject *parent);

    void notificationCallback(const QMqttMessage &message);
};

Notifications::Notifications(QObject *parent):
    Entity(parent)
{
    setId("notifications");
    setName("Notifications");

    connect(HaControl::mqttClient(), &QMqttClient::connected, this, [this]() {
        auto watcher = HaControl::mqttClient()->subscribe(baseTopic());
        connect(watcher, &QMqttSubscription::messageReceived, this, &Notifications::notificationCallback);
    });
}

void Notifications::notificationCallback(const QMqttMessage &message)
{
    auto docs = QJsonDocument::fromJson(message.payload());
    auto objs = docs.object();
    const QString title = objs["title"].toString();
    const QString body = objs["message"].toString();
    KNotification::event(KNotification::Notification, title, body);
}

void setupNotifications()
{
    new Notifications(qApp);
}

REGISTER_INTEGRATION("Notifications",setupNotifications,true)
#include "notifications.moc"
