// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "select.h"
#include "Transport/transportmanager.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QMqttClient>
#include <QMqttSubscription>


DEFINE_LOGGER(sel, Shared.Entities.Select)

using KIOTShared::Transport::TransportManager;

namespace KIOTShared {
namespace Entities {


Select::Select(QObject *parent)
    : Entity(parent)
{
    setHaType("select");
}

void Select::setOptions(const QStringList &opts)
{
    m_options = opts;

    // Hvis HA er registrert, må config oppdateres
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("command_topic", baseTopic() + "/set");
    setDiscoveryConfig("options", QJsonArray::fromStringList(m_options));
    sendRegistration();
}

void Select::setState(const QString &state)
{
    m_state = state;
    publishState();
}

QString Select::state() const
{
    return m_state;
}

QStringList Select::options() const
{
    return m_options;
}
void Select::init()
{
    // Startkonfig for HA
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("command_topic", baseTopic() + "/set");
    setDiscoveryConfig("options", QJsonArray::fromStringList(m_options));

    // Fortell HA at entiteten finnes
    sendRegistration();

    // Publiser initial state hvis satt
    publishState();

    // Unsubscribe først for å unngå delte subscriptions

    // Opprett lokal subscription
    auto subscription = TransportManager::mqttClient() ->subscribe(baseTopic() + "/set");
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
            const QString newValue = QString::fromUtf8(message.payload());
            qCDebug(sel) << "Received new value for " << name() << ": " << newValue;
            if (!m_options.contains(newValue)) {
                qCWarning(sel) << name() << "ignores" << newValue << "- not one of its options" << m_options;
                return;
            }
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
    if (TransportManager::mqttClient() ->state() != QMqttClient::Connected)
        return;

    TransportManager::mqttClient() ->publish(baseTopic(), m_state.toUtf8(), 0, true);
}
} // namespace Entities
} // namespace KIOTShared