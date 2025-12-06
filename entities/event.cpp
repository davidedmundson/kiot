// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "event.h"
#include "core.h"
#include <QMqttClient>
Event::Event(QObject *parent)
    : Entity(parent)
{
}

void Event::init()
{
    setHaType("device_automation");
    setHaConfig({
        {"automation_type", "trigger"},
        {"topic", baseTopic()},
        {"type", {"button_short_press"}},
        {"subtype", name()}
    });
    sendRegistration();
}

void Event::trigger()
{
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), "pressed", 0, false);
        HaControl::mqttClient()->publish(baseTopic(), "", 0, true);
    }
}
