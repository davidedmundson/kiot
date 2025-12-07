// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "binarysensor.h"
#include "core.h"
#include <QMqttClient>
BinarySensor::BinarySensor(QObject *parent)
    : Entity(parent)
{
}

void BinarySensor::init()
{
    setHaType("binary_sensor");
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("payload_on", "true");
    setDiscoveryConfig("payload_off", "false");
    sendRegistration();
    publish();
}

void BinarySensor::publish()
{
    qDebug() << name() << "publishing state" << m_state;
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), m_state ? "true" : "false", 0, true);
    }
}

void BinarySensor::setState(bool state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    publish();
}

bool BinarySensor::state() const
{
    return m_state;
}
