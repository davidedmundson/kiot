// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "entity.h"
#include "core.h"
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttClient>

Entity::Entity(QObject *parent):
    QObject(parent)
{
    connect(HaControl::mqttClient(), &QMqttClient::connected, this, &Entity::init);
}

QString Entity::hostname() const
{
    return QHostInfo::localHostName().toLower();
}

QString Entity::baseTopic() const
{
    return hostname() + "/" + id();
}

QString Entity::haType() const
{
    return m_haType;
}

void Entity::setHaType(const QString &newHaType)
{
    m_haType = newHaType;
}

QString Entity::name() const
{
    return m_name;
}

void Entity::setDiscoveryConfig(const QString &key, const QVariant &value)
{
    m_haConfig[key] = value;
}

void Entity::setName(const QString &newName)
{
    m_name = newName;
}

void Entity::setHaIcon(const QString &newHaIcon)
{
    m_haIcon = newHaIcon;
    sendRegistration();
}

QString Entity::haIcon() const
{
    return  m_haIcon;
}

// TODO This needs a universal global check making sure we have unique ids to avoid problems with mqtt
QString Entity::id() const
{
    return m_id;
}

void Entity::setId(const QString &newId)
{
    m_id = newId;
}

void Entity::init()
{}
static QString s_discoveryPrefix = "homeassistant";
void Entity::sendRegistration()
{
    if (haType().isEmpty()) {
        return;
    }
    QVariantMap config = m_haConfig;
    config["name"] = name();
    
    if (id() != "connected") { //special case
        config["availability_topic"] = hostname() + "/connected";
        config["payload_available"] = "on";
        config["payload_not_available"] = "off";
        if (!haIcon().isEmpty()){
            config["icon"] = haIcon();
    
        }
    }
    if (!config.contains("device")) {
        config["device"] = QVariantMap({{"identifiers", "linux_ha_bridge_" + hostname() }});
    }
    config["unique_id"] = "linux_ha_control_"+ hostname() + "_" + id();
    HaControl::mqttClient()->publish(s_discoveryPrefix + "/" + haType() + "/" + hostname() + "/" + id() + "/config", QJsonDocument(QJsonObject::fromVariantMap(config)).toJson(QJsonDocument::Compact), 0, true);
    if (id() != "connected") { //special case
        HaControl::mqttClient()->publish(hostname() + "/connected", "on", 0, false);
    }
}
