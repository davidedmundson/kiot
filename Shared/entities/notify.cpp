// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file notify.cpp
 * @brief Implementation of the MQTT Notify entity for Home Assistant
 *
 * @details
 * This file implements the Notify class which provides notification
 * receiving functionality for the KIOT project. It allows receiving
 * messages from Home Assistant via MQTT that can trigger various
 * actions on the computer.
 *
 * Based on Home Assistant's MQTT notify integration documentation:
 * https://www.home-assistant.io/integrations/notify.mqtt/
 *
 * The notify entity can be used in Home Assistant automations to send
 * messages to the computer, which can then be processed for:
 * - Text-to-speech announcements (e.g., using Qt's QTextToSpeech)
 * - Desktop notifications via the system notification framework
 * - Custom automation triggers for local scripts or applications
 * - Audio alerts or system sounds
 */

#include "notify.h"
#include "Transport/transportmanager.h"
#include <QMqttClient>


DEFINE_LOGGER(notify, entities.Notify)

using KIOTShared::Transport::TransportManager;

namespace KIOTShared {
namespace Entities {


Notify::Notify(QObject *parent)
    : Entity(parent)
{
}

void Notify::init()
{
    setHaType("notify");

    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("command_topic", baseTopic() + "/notifications");

    sendRegistration();

    auto subscription = TransportManager::mqttClient() ->subscribe(baseTopic() + "/notifications");
    if (!subscription) {
        return;
    }
    connect(subscription, &QMqttSubscription::messageReceived, this, [this](QMqttMessage message) {
        qCDebug(notify) << "Notify message received" << QString::fromUtf8(message.payload());
        emit notificationReceived(message.payload());
    });
}
} // namespace Entities
} // namespace KIOTShared