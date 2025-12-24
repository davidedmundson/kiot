// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sensor.h"
#include "core.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttClient>
Sensor::Sensor(QObject *parent)
    : Entity(parent)
{
}

void Sensor::init()
{
    setHaType("sensor");

    // Standard state topic
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("json_attributes_topic", baseTopic() + "/attributes"); // ny topic for attributes

    sendRegistration();
    publishState();
    publishAttributes();
}

void Sensor::setState(const QString &state)
{
    m_state = state;
    publishState();
}

void Sensor::publishState()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    HaControl::mqttClient()->publish(baseTopic(), m_state.toUtf8(), 0, true);
}
