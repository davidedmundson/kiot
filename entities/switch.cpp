// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "switch.h"
#include "core.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttClient>
#include <QMqttSubscription>

Switch::Switch(QObject *parent)
    : Entity(parent)
{
    setHaType("switch");
}

void Switch::init()
{
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("command_topic", baseTopic() + "/set");
    setDiscoveryConfig("payload_on", "true");
    setDiscoveryConfig("payload_off", "false");
    setDiscoveryConfig("json_attributes_topic", baseTopic() + "/attributes");

    sendRegistration();
    setState(m_state);

    auto subscription = HaControl::mqttClient()->subscribe(baseTopic() + "/set");
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
            if (message.payload() == "true") {
                Q_EMIT stateChangeRequested(true);
            } else if (message.payload() == "false") {
                Q_EMIT stateChangeRequested(false);
            } else {
                qWarning() << "unknown state request" << message.payload();
            }
        });
    }
}
void Switch::setState(bool state)
{
    m_state = state;
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), state ? "true" : "false", 0, true);
    }
}
void Switch::setAttributes(const QVariantMap &attrs)
{
    m_attributes = attrs;
    publishAttributes();
}
void Switch::publishAttributes()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    QJsonObject obj;
    for (auto it = m_attributes.constBegin(); it != m_attributes.constEnd(); ++it)
        obj[it.key()] = QJsonValue::fromVariant(it.value());

    QJsonDocument doc(obj);
    HaControl::mqttClient()->publish(baseTopic() + "/attributes", doc.toJson(QJsonDocument::Compact), 0, true);
}
