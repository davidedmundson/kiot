// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "battery.h"
#include "core.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttClient>
Battery::Battery(QObject *parent)
    : Entity(parent)
{
}

void Battery::init()
{
    setHaType("sensor");

    // Standard state topic
    setHaConfig({
        {"state_topic", baseTopic()},
        {"unit_of_measurement", "%"},
        {"device_class", "battery"},
        {"json_attributes_topic", baseTopic() + "/attributes"}
    });

    sendRegistration();
    publishState();
    publishAttributes();
}

void Battery::setState(const int &state)
{
    m_state = state;
    publishState();
}
int Battery::getState()
{
    return  m_state;
}
void Battery::setAttributes(const QVariantMap &attrs)
{
    m_attributes = attrs;
    publishAttributes();
}
QVariantMap Battery::getAttributes()
{
    return m_attributes;
}
void Battery::publishState()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    HaControl::mqttClient()->publish(baseTopic(), QString::number(m_state).toUtf8(), 0, true);
}

void Battery::publishAttributes()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    QJsonObject obj;
    for (auto it = m_attributes.constBegin(); it != m_attributes.constEnd(); ++it)
        obj[it.key()] = QJsonValue::fromVariant(it.value());

    QJsonDocument doc(obj);
    HaControl::mqttClient()->publish(baseTopic() + "/attributes", doc.toJson(QJsonDocument::Compact), 0, true);
}
