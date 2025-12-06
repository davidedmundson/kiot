// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "button.h"
#include "core.h"
#include <QMqttSubscription>
#include <QMqttClient>

Button::Button(QObject *parent)
    : Entity(parent)
{
}

void Button::init()
{
    setHaType("button");
    setHaConfig({
        {"command_topic", baseTopic()}
    });
    sendRegistration();

    auto subscription = HaControl::mqttClient()->subscribe(baseTopic());
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &){
            Q_EMIT triggered();
        });
    }
}
