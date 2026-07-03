// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file camera.h
 * @brief MQTT Camera entity for Home Assistant integration
 * 
 * @details
 * This header defines the Camera class which implements a camera entity
 * for the KIOT project. The camera entity allows publishing image snapshots
 * to Home Assistant via MQTT using base64 encoding.
 * 
 * Based on Home Assistant's MQTT camera integration documentation:
 * https://www.home-assistant.io/integrations/camera.mqtt/
 * 
 * @note This implementation is designed for snapshot images, not live streaming.
 *       It can be triggered via MQTT commands to update the camera image.
 */

#pragma once
#include "entity.h"
#include <QObject>
#include <QByteArray>

/**
 * @class Camera
 * @brief Camera entity for publishing image snapshots to Home Assistant
 * 
 * @details
 * This class extends the Entity base class to implement a camera entity
 * that can publish image data to Home Assistant via MQTT. The images are
 * encoded in base64 format and can be triggered to update via MQTT commands.
 * 
 * The camera entity is designed for snapshot functionality rather than
 * live streaming. It includes support for command-based image updates
 * through a custom command topic.
 * 
 * @note Inherits from Entity to leverage MQTT discovery and topic management
 */
class Camera : public Entity
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a Camera entity
     * @param parent Parent QObject (optional)
     * 
     * @details
     * Initializes the Camera entity. The actual MQTT setup is performed
     * in the init() method when the MQTT client connects.
     */
    Camera(QObject *parent = nullptr);
    
    /**
     * @brief Publishes an image to Home Assistant
     * @param imageDataBase64 Image data encoded in base64 format
     * 
     * @details
     * Publishes the provided base64-encoded image data to the camera's
     * MQTT state topic. Also updates attributes with metadata such as
     * timestamp and image size.
     * 
     * @note Only publishes if the MQTT client is connected
     * @note Image data should be in a format compatible with Home Assistant's
     *       MQTT camera integration (typically JPEG or PNG)
     */
    void publishImage(const QByteArray &imageDataBase64);

Q_SIGNALS:
    /**
     * @brief Signal emitted when a camera command is received
     * @param command The command string received from Home Assistant
     * 
     * @details
     * This is not a part of Home Assistants MQTT camera integration, 
     * but a custom signal to allow triggers for requesting image updates.
     * Emitted when a command is received on the camera's command topic.
     * Integrations can connect to this signal to trigger image updates
     * when Home Assistant requests a fresh image.
     */
    void commandReceived(const QString &command);
    
protected:
    /**
     * @brief Initializes the camera entity with MQTT configuration
     * 
     * @details
     * Sets up the camera entity for Home Assistant MQTT discovery:
     * - Configures the entity type as "camera"
     * - Sets the state topic for image publishing
     * - Configures base64 image encoding
     * - Sets up a command topic for triggering image updates
     * - Subscribes to the command topic to receive update requests
     * 
     * The command topic functionality is a custom extension beyond the
     * standard Home Assistant MQTT camera integration, allowing integrations
     * to trigger image updates via MQTT commands.
     * 
     * @note The command topic is not part of the standard Home Assistant
     *       MQTT camera integration but provides flexibility for custom
     *       integrations to request fresh images.
     */
    void init() override;
};