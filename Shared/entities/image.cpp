// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file image.cpp
 * @brief Implementation of the MQTT Image entity for Home Assistant
 *
 * @details
 * This file implements the Image class which provides static image entity
 * functionality for the KIOT project.
 *
 * Based on Home Assistant's MQTT image integration documentation:
 * https://www.home-assistant.io/integrations/image.mqtt/
 */

#include "image.h"
#include "platformhelper.h"
#include "Transport/transportmanager.h"
#include <QMqttClient>
#include <QDateTime>

DEFINE_LOGGER(imgentity,Shared.Entities.Image)

using KIOTShared::Transport::TransportManager;

namespace KIOTShared {
namespace Entities {



Image::Image(QObject *parent)
    : Entity(parent)
{
}
void Image::setMimeType(const QString &mimeType)
{
    if(m_mimeType == mimeType)
       return;
    m_mimeType = mimeType;
    
}
void Image::setIsUrlMode(bool isUrlMode)
{
    m_isUrlMode = isUrlMode;

}
void Image::init()
{
    setHaType("image");
    // MQTT Image supports either image_topic or url_topic. 
    if(m_isUrlMode)
    {
        qCDebug(imgentity) << "Image entity is in URL mode";
        setDiscoveryConfig("url_topic", baseTopic() + "/url");

    } else {
        qCDebug(imgentity) << "Image entity is in image mode";
        setDiscoveryConfig("image_topic", baseTopic() + "/image");
        setDiscoveryConfig("image_encoding", "b64");
        setDiscoveryConfig("content_type", m_mimeType);
    }
    // Støtte for eventuell url_topic om enheten heller vil publisere URL-er

    sendRegistration();
}

void Image::publishImage(const QByteArray &imageDataBase64)
{
    if (TransportManager::mqttClient() ->state() != QMqttClient::Connected)
        return;
    if(m_isUrlMode)
    {
        qCWarning(imgentity) << "Image entity is in URL mode, cannot publish image data";
        return;
    }
    // Publiserer selve bildeinnholdet til image_topic
    TransportManager::mqttClient() ->publish(baseTopic() + "/image", imageDataBase64, 0, true);

    // Oppdater standard attributter via Entity-baseklassen
    QVariantMap attrs;
    attrs["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    attrs["size_bytes"] = imageDataBase64.size();
    setAttributes(attrs);
}

void Image::publishImageUrl(const QString &imageUrl)
{
    if (TransportManager::mqttClient() ->state() != QMqttClient::Connected)
        return;
    if(!m_isUrlMode)
    {
        qCWarning(imgentity) << "Image entity is in image mode, cannot publish url";
        return;
    }

    TransportManager::mqttClient() ->publish(baseTopic() + "/url", imageUrl.toUtf8(), 0, true);

    QVariantMap attrs;
    attrs["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    attrs["image_url"] = imageUrl;
    setAttributes(attrs);
}

} // namespace Entities
} // namespace KIOTShared