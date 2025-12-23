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
     * Emitted when a command is received on the camera's command topic.
     * Integrations can connect to this signal to trigger image updates
     * when Home Assistant requests a fresh image.
     */
    void commandReceived(const QString &command);
    
protected:
    /**
     * @brief Initializes the camera entity
     * 
     * @details
     * Overrides Entity::init() to set up camera-specific MQTT configuration:
     * - Sets HA type to "camera"
     * - Configures image encoding as base64
     * - Sets up state and command topics
     * - Subscribes to command topic for image update requests
     * 
     * Called automatically when the MQTT client connects.
     */
    void init() override;
};