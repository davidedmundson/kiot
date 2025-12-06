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
    setHaConfig({
        {"state_topic", baseTopic()},
        {"json_attributes_topic", baseTopic() + "/attributes"} // ny topic for attributes
    });

    sendRegistration();
    publishState();
    publishAttributes();
}

void Sensor::setState(const QString &state)
{
    m_state = state;
    publishState();
}

void Sensor::setAttributes(const QVariantMap &attrs)
{
    m_attributes = attrs;
    publishAttributes();
}

void Sensor::publishState()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    HaControl::mqttClient()->publish(baseTopic(), m_state.toUtf8(), 0, true);
}

void Sensor::publishAttributes()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    QJsonObject obj;
    for (auto it = m_attributes.constBegin(); it != m_attributes.constEnd(); ++it)
        obj[it.key()] = QJsonValue::fromVariant(it.value());

    QJsonDocument doc(obj);
    HaControl::mqttClient()->publish(baseTopic() + "/attributes", doc.toJson(QJsonDocument::Compact), 0, true);
}
