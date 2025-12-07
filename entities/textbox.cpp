// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "textbox.h"
#include "core.h"
#include <QMqttSubscription>
#include <QMqttClient>
#include <QJsonDocument>
#include <QJsonObject>


Textbox::Textbox(QObject *parent)
    : Entity(parent)
{
    setHaType("text");
}

void Textbox::init()
{
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("command_topic", baseTopic() + "/set");
    setDiscoveryConfig("json_attributes_topic", baseTopic() + "/attributes");

    sendRegistration();
    setState(m_text);

    auto subscription = HaControl::mqttClient()->subscribe(baseTopic() + "/set");
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
            QString newText = QString::fromUtf8(message.payload());
            m_text = newText;
            emit stateChangeRequested(m_text);
            setState(m_text); // oppdater MQTT state
        });
    }
}

void Textbox::setState(const QString &text)
{
    m_text = text;
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), text.toUtf8(), 0, true);
    }
}
