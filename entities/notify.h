// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file notify.h
 * @brief MQTT Notify entity for Home Assistant integration
 * 
 * @details
 * This header defines the Notify class which implements a notify entity
 * for the KIOT project. The notify entity allows receiving notification
 * messages from Home Assistant via MQTT, which can be used to trigger
 * various actions on the computer such as text-to-speech, desktop
 * notifications, or custom automation triggers.
 * 
 * Based on Home Assistant's MQTT notify integration documentation:
 * https://www.home-assistant.io/integrations/notify.mqtt/
 * 
 * The notify entity can be used with Home Assistant automations like:
 * @code
 * action: notify.send_message
 * data:
 *   message: A message for you
 *   entity_id: notify.kiot_entityid_notify
 * @endcode
 */

#pragma once
#include "entity.h"

/**
 * @class Notify
 * @brief Notify entity for receiving messages from Home Assistant
 * 
 * @details
 * This class extends the Entity base class to implement a notify entity
 * that can receive messages from Home Assistant via MQTT. These messages
 * can trigger various actions on the computer, such as:
 * - Text-to-speech announcements (using QTextToSpeech)
 * - Desktop notifications
 * - Custom automation triggers
 * - System alerts or reminders
 * 
 * The entity subscribes to a command topic and emits a signal when
 * notifications are received, allowing integrations to react to
 * Home Assistant messages.
 * 
 * @note Inherits from Entity to leverage MQTT discovery and topic management
 */
class Notify : public Entity
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a Notify entity
     * @param parent Parent QObject (optional)
     */
    Notify(QObject *parent = nullptr);

protected:
    /**
     * @brief Initializes the notify entity
     * 
     * @details
     * Overrides Entity::init() to set up notify-specific MQTT configuration:
     * - Sets HA type to "notify"
     * - Configures state and command topics
     * - Subscribes to the notifications command topic
     * - Sets up signal connection for received messages
     * 
     * Called automatically when the MQTT client connects.
     */
    void init() override;
    
Q_SIGNALS:
    /**
     * @brief Signal emitted when a notification message is received
     * @param message The notification message received from Home Assistant
     * 
     * @details
     * Emitted when a notification message is received on the notify entity's
     * command topic. Integrations can connect to this signal to process
     * incoming messages from Home Assistant, such as:
     * - Speaking the message using text-to-speech
     * - Displaying desktop notifications
     * - Triggering custom automation scripts
     * - Logging or processing the message content
     * 
     * The message format is determined by Home Assistant's notify service
     * and can include plain text or structured data.
     */
    void notificationReceived(const QString &message);
};