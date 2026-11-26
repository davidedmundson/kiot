// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "number.h"
#include "Transport/transportmanager.h"
#include <QMqttClient>


DEFINE_LOGGER(numb, Shared.Entities.Number)

using KIOTShared::Transport::TransportManager;

namespace KIOTShared {
namespace Entities {


Number::Number(QObject *parent)
    : Entity(parent)
{
    setHaType("number");
    setRange(m_min, m_max, m_step, m_unit);
}

void Number::setRange(int min, int max, int step, const QString &unit)
{
    m_min = min;
    m_max = max;
    m_step = step;
    m_unit = unit;
    setDiscoveryConfig("min", QString::number(m_min));
    setDiscoveryConfig("max", QString::number(m_max));
    setDiscoveryConfig("step", QString::number(m_step));
    setDiscoveryConfig("unit_of_measurement", m_unit);
}

void Number::init()
{
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("command_topic", baseTopic() + "/set");

    sendRegistration();

    setValue(m_value);

    auto subscription = TransportManager::mqttClient() ->subscribe(baseTopic() + "/set");
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
            bool ok = false;
            int newValue = message.payload().toInt(&ok);
            if (ok && (newValue < m_min || newValue > m_max)) {
                qCWarning(numb) << name() << "ignores" << newValue << "- outside" << m_min << "to" << m_max;
            } else if (ok) {
                Q_EMIT valueChangeRequested(newValue);
            } else {
                qCWarning(numb) << "Invalid payload for number entity:" << message.payload();
            }
        });
    }
}

void Number::setValue(int value)
{
    m_value = value;
    if (TransportManager::mqttClient() ->state() == QMqttClient::Connected) {
        TransportManager::mqttClient() ->publish(baseTopic(), QByteArray::number(value), 0, true);
    }
}

int Number::value()
{
    return m_value;
}
} // namespace Entities
} // namespace KIOTShared