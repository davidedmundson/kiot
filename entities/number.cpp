// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "number.h"
#include "core.h"
#include <QMqttClient>
Number::Number(QObject *parent)
    : Entity(parent)
{
    setHaType("number");
}

void Number::setRange(int min, int max, int step, const QString &unit)
{
    m_min = min;
    m_max = max;
    m_step = step;
    m_unit = unit;
}

void Number::init()
{
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("command_topic", baseTopic() + "/set");
    setDiscoveryConfig("min", QString::number(m_min));
    setDiscoveryConfig("max", QString::number(m_max));
    setDiscoveryConfig("step", QString::number(m_step));
    setDiscoveryConfig("unit_of_measurement", m_unit);

    sendRegistration();

    setValue(m_value);

    auto subscription = HaControl::mqttClient()->subscribe(baseTopic() + "/set");
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
            bool ok = false;
            int newValue = message.payload().toInt(&ok);
            if (ok) {
                Q_EMIT valueChangeRequested(newValue);
            } else {
                qWarning() << "Invalid payload for number entity:" << message.payload();
            }
        });
    }
}

void Number::setValue(int value)
{
    m_value = value;
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), QByteArray::number(value), 0, true);
    }
}

int Number::value()
{
    return m_value;
}
