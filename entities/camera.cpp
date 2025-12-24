// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file camera.cpp
 * @brief Implementation of the MQTT Camera entity for Home Assistant
 *
 * @details
 * This file implements the Camera class which provides camera snapshot
 * functionality for the KIOT project. It allows publishing base64-encoded
 * images to Home Assistant via MQTT and supports command-triggered updates.
 *
 * Based on Home Assistant's MQTT camera integration documentation:
 * https://www.home-assistant.io/integrations/camera.mqtt/
 *
 * @note This implementation is designed for snapshot images, not live streaming.
 *       It can be triggered via MQTT commands to update the camera image.
 */

#include "camera.h"
#include "core.h"
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMqttClient>
Q_DECLARE_LOGGING_CATEGORY(camentity)
Q_LOGGING_CATEGORY(camentity, "entities.Camera")

Camera::Camera(QObject *parent)
    : Entity(parent)
{
}

void Camera::init()
{
    setHaType("camera");
    setDiscoveryConfig("topic", baseTopic());
    setDiscoveryConfig("image_encoding", "b64");
    // This is not supported by default, but can be used to publish a trigger command to update camera image from HA
    setDiscoveryConfig("command_topic", baseTopic() + "/command");
    sendRegistration();

    // This is not supported by default from Home Assistant's MQTT camera integration,
    // but lets you publish a command and use it from the signal to trigger a fresh image in a integration
    auto subscription = HaControl::mqttClient()->subscribe(baseTopic() + "/command");
    connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
        qCDebug(camentity) << name() << "Camera command received:" << QString::fromUtf8(message.payload());
        emit commandReceived(QString::fromUtf8(message.payload()));
    });
}

void Camera::publishImage(const QByteArray &imageDataBase64)
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    HaControl::mqttClient()->publish(baseTopic(), imageDataBase64, 0, true);

    QVariantMap attrs;
    attrs["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    attrs["size_bytes"] = imageDataBase64.size();
    setAttributes(attrs);
}
