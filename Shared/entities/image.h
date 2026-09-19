// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file image.h
 * @brief MQTT Image entity for Home Assistant integration
 *
 * @details
 * This header defines the Image class which implements an image entity
 * for the KIOT project. The image entity allows publishing image data or 
 * image URLs to Home Assistant via MQTT using base64 encoding or raw binary.
 *
 * Based on Home Assistant's MQTT image integration documentation:
 * https://www.home-assistant.io/integrations/image.mqtt/
 */

#pragma once
#include "entity.h"
#include <QByteArray>
#include <QObject>

/**
 * @class Image
 * @brief Image entity for publishing static images or image URLs to Home Assistant
 *
 * @details
 * This class extends the Entity base class to implement an image entity
 * that can publish image snapshots or content types to Home Assistant via MQTT.
 */
class Image : public Entity
{
    Q_OBJECT
public:
    /**
     * @brief Constructs an Image entity
     * @param parent Parent QObject (optional)
     */
    Image(QObject *parent = nullptr);

    /**
     * @brief Sets the MIME type of the image
     * @param mimeType The MIME type string (e.g., "image/jpeg", "image/png")
     *
     * @details
     * Configures the content type expected by Home Assistant when receiving
     * binary image data.
     */
    void setMimeType(const QString &mimeType);

    /**
     * @brief Sets whether the image entity operates in URL mode or binary image mode
     * @param isUrlMode True to use URL mode (`url_topic`), false for binary data (`image_topic`)
     *
     * @details
     * Must be configured prior to entity registration, as Home Assistant treats
     * `image_topic` and `url_topic` as mutually exclusive options.
     */
    void setIsUrlMode(bool isUrlMode = false);

    /**
     * @brief Publishes image binary data (base64 encoded) to Home Assistant
     * @param imageDataBase64 Image data encoded in base64 format
     *
     * @details
     * Publishes the raw or base64-encoded image payload to the `image_topic`.
     * Fails with a warning if the entity is currently set to URL mode.
     */
    void publishImage(const QByteArray &imageDataBase64);

    /**
     * @brief Publishes an image URL to Home Assistant
     * @param imageUrl The URL pointing to the image file
     *
     * @details
     * Publishes the image URL to the `url_topic`.
     * Fails with a warning if the entity is not in URL mode.
     */
    void publishImageUrl(const QString &imageUrl);

protected:
    /**
     * @brief Initializes the image entity
     *
     * @details
     * Overrides Entity::init() to set up image-specific MQTT configuration:
     * - Sets HA type to "image"
     * - Dynamically configures either `image_topic` (with base64 encoding and content type)
     *   or `url_topic` based on `m_isUrlMode`.
     */
    void init() override;

private:
    QString m_mimeType = QStringLiteral("image/jpeg");  ///< MIME type for image data payload
    bool m_isUrlMode = false;                          ///< Flag indicating if entity uses URL mode instead of binary image topic
};