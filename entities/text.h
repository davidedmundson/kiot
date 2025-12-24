// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file text.h
 * @brief Text entity for Home Assistant integration
 *
 * @details
 * This header defines the Text class which implements a text entity
 * for Home Assistant. Text entities provide a text input interface for
 * entering and editing string values, with bidirectional text synchronization
 * between KIOT and Home Assistant.
 *
 * The text entity is commonly used for script argument input, custom
 * command entry, or any scenario requiring text input from Home Assistant.
 */

#pragma once

#include "entity.h"

/**
 * @class Text
 * @brief Text entity for text input and editing
 *
 * @details
 * The Text class represents a text entity in Home Assistant, used for
 * text input and editing. It supports bidirectional text synchronization:
 * text changes from KIOT are reported to Home Assistant, and text changes
 * from Home Assistant trigger text change requests in KIOT.
 *
 * Common use cases:
 * - Script argument input (e.g., URLs, commands, parameters)
 * - Custom command entry
 * - Text-based configuration
 * - Note or message input
 *
 * @note Inherits from Entity to leverage MQTT discovery and topic management.
 */
class Text : public Entity
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a Text entity
     * @param parent Parent QObject for memory management (optional)
     */
    explicit Text(QObject *parent = nullptr);

    /**
     * @brief Initializes the text entity
     *
     * @details
     * Overrides Entity::init() to set up text-specific configuration,
     * including command topic subscription for receiving text change
     * commands from Home Assistant.
     */
    void init() override;

    /**
     * @brief Sets the current text content
     * @param text Text content to set
     *
     * @details
     * Sets the current text content and automatically publishes it to
     * Home Assistant. The text can be any string value.
     */
    void setState(const QString &text);

    /**
     * @brief Gets the current text content
     * @return QString Current text content
     */
    QString state() const
    {
        return m_text;
    }

Q_SIGNALS:
    /**
     * @brief Signal emitted when text change is requested from Home Assistant
     * @param text New text content requested
     *
     * @details
     * Emitted when Home Assistant sends a command to change the text content.
     * Integrations should connect to this signal to process the new text,
     * such as using it as an argument for script execution.
     *
     * Example with script integration:
     * @code
     * connect(text, &Text::stateChangeRequested, [](const QString &arg) {
     *     // Use text as argument for script
     *     executeScriptWithArgument(arg);
     * });
     * @endcode
     */
    void stateChangeRequested(const QString &text);

private:
    /** @private Current text content */
    QString m_text;
};