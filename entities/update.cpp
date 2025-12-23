// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

// Based on Home Assistant's MQTT update integration documentation:
// https://www.home-assistant.io/integrations/update.mqtt/
#include "update.h"
#include "core.h"
#include <QMqttSubscription>
#include <QMqttClient>
#include <QJsonDocument>
#include <QJsonObject>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(upd)
Q_LOGGING_CATEGORY(upd, "entities.Update")

Update::Update(QObject *parent)
    : Entity(parent)
{
    setHaType("update");
}

void Update::init()
{
    // Set up discovery configuration for update entity
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("command_topic", baseTopic() + "/set");
    setDiscoveryConfig("payload_install", "install");
    
    // Send registration to Home Assistant
    sendRegistration();
    
    // Publish initial state
    publishState();
    
    // Subscribe to command topic for installation requests
    auto subscription = HaControl::mqttClient()->subscribe(baseTopic() + "/set");
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
            if (message.payload() == "install") {
                Q_EMIT installRequested();
            } else {
                qCWarning(upd) << "Unknown update command:" << message.payload();
            }
        });
    }
}

void Update::setInstalledVersion(const QString &version)
{
    if (m_installedVersion != version) {
        m_installedVersion = version;
        publishState();
    }
}

void Update::setLatestVersion(const QString &version)
{
    if (m_latestVersion != version) {
        m_latestVersion = version;
        publishState();
    }
}

void Update::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        publishState();
    }
}

void Update::setReleaseSummary(const QString &summary)
{
    if (m_releaseSummary != summary) {
        m_releaseSummary = summary;
        publishState();
    }
}

void Update::setReleaseUrl(const QString &url)
{
    if (m_releaseUrl != url) {
        m_releaseUrl = url;
        publishState();
    }
}

void Update::setEntityPicture(const QString &url)
{
    if (m_entityPicture != url) {
        m_entityPicture = url;
        publishState();
    }
}

void Update::setInProgress(bool inProgress)
{
    if (m_inProgress != inProgress) {
        if(!inProgress)
            m_updatePercentage = -1;
        m_inProgress = inProgress;
        publishState();
    }
}

void Update::setUpdatePercentage(int percentage)
{
    // Validate percentage range (-1 to clear, 0-100 for progress)
    if (percentage < -1 || percentage > 100) {
        qCWarning(upd) << "Invalid update percentage:" << percentage << "(must be -1 to 100)";
        return;
    }
    
    if (m_updatePercentage != percentage) {
        m_updatePercentage = percentage;
        publishState();
    }
}

void Update::publishState()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    // Build JSON payload according to Home Assistant update entity schema
    QJsonObject payload;
    
    // Required fields
    if (!m_installedVersion.isEmpty()) {
        payload["installed_version"] = m_installedVersion;
    }
    
    if (!m_latestVersion.isEmpty()) {
        payload["latest_version"] = m_latestVersion;
    }
    
    // Optional fields
    if (!m_title.isEmpty()) {
        payload["title"] = m_title;
    }
    
    if (!m_releaseSummary.isEmpty()) {
        payload["release_summary"] = m_releaseSummary;
    }
    
    if (!m_releaseUrl.isEmpty()) {
        payload["release_url"] = m_releaseUrl;
    }
    
    if (!m_entityPicture.isEmpty()) {
        payload["entity_picture"] = m_entityPicture;
    }
    
    payload["in_progress"]  = m_inProgress ? "true" : "false";
    
    if (m_inProgress && m_updatePercentage >= 0) {
        payload["update_percentage"] = m_updatePercentage;
    } else {
        payload["update_percentage"] = QJsonValue(); // clear it in HA
    }
    

    QJsonDocument doc(payload);
    HaControl::mqttClient()->publish(baseTopic(), doc.toJson(QJsonDocument::Compact), 0, true);
}