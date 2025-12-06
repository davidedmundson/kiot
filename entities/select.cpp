// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "select.h"
#include "core.h"
#include <QMqttClient>
#include <QMqttSubscription>
#include <QJsonDocument>
#include <QJsonArray>

Select::Select(QObject *parent)
    : Entity(parent)
{
    setHaType("select");
}

void Select::setOptions(const QStringList &opts)
{
    m_options = opts;

    // Hvis HA er registrert, må config oppdateres
    setHaConfig({
        {"state_topic", baseTopic()},
        {"command_topic", baseTopic() + "/set"},
        {"options", QJsonArray::fromStringList(m_options)}
    });
    sendRegistration();
}

void Select::setState(const QString &state)
{
    m_state = state;
    publishState();
}

QString Select::getState() const
{
    return m_state;
}

QStringList Select::getOptions() const
{
    return m_options;
}
void Select::init()
{
    // Startkonfig for HA
    setHaConfig({
        {"state_topic", baseTopic()},
        {"command_topic", baseTopic() + "/set"},
        {"options", QJsonArray::fromStringList(m_options)}
    });

    // Fortell HA at entiteten finnes
    sendRegistration();

    // Publiser initial state hvis satt
    publishState();

    // Unsubscribe først for å unngå delte subscriptions


    // Opprett lokal subscription
    auto subscription = HaControl::mqttClient()->subscribe(baseTopic() + "/set");
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
            const QString newValue = QString::fromUtf8(message.payload());
            qDebug() << "Received new value for " << name() << ": " << newValue;
            // Oppdater lokalt
            m_state = newValue;
            publishState();

            // Varsle integrasjonen
            emit optionSelected(m_state);
        });
    }
}


void Select::publishState()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    HaControl::mqttClient()->publish(baseTopic(), m_state.toUtf8(), 0, true);
}
