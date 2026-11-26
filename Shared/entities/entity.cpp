// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file entity.cpp
 * @brief Base Entity class implementation for KIOT Home Assistant integration
 * 
 * @details
 * This file implements the Entity base class which provides common functionality
 * for all KIOT entities that integrate with Home Assistant via MQTT.
 * 
 * The Entity class handles:
 * - MQTT topic management and discovery
 * - Attribute publishing and conversion for Home Assistant compatibility
 * - Connection state management
 * - Unique identification and device association
 */

#include "entity.h"
#include "Transport/transportmanager.h"
#include <QCryptographicHash>
#include <QHash>
#include <QHostInfo>
#include <QSysInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMqttClient>
#include <algorithm>
//#include <KConfigGroup>


DEFINE_LOGGER(base, Shared.Entities.Entity)
using KIOTShared::Transport::TransportManager;
namespace KIOTShared {
namespace Entities {

// Home Assistant accepts only [a-zA-Z0-9_-] as the node and object id of a discovery topic (TOPIC_MATCHER
// in homeassistant/components/mqtt/discovery.py). A letter with an accent or umlaut becomes its base
// letter, a few letters that don't decompose that way are spelled out, anything else becomes '_'.
static QString sanitizeForMqttTopic(const QString &input)
{
    static const QHash<QChar, QString> spelledOut = {
        {QChar(0x00DF), QStringLiteral("ss")}, // ß
        {QChar(0x00E6), QStringLiteral("ae")}, // æ
        {QChar(0x00F8), QStringLiteral("o")}, // ø
        {QChar(0x0153), QStringLiteral("oe")}, // œ
        {QChar(0x0111), QStringLiteral("d")}, // đ
        {QChar(0x00F0), QStringLiteral("d")}, // ð
        {QChar(0x0142), QStringLiteral("l")}, // ł
        {QChar(0x00FE), QStringLiteral("th")}, // þ
    };
    QString result;
    const QString decomposed = input.toLower().normalized(QString::NormalizationForm_KD);
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing) {
            continue; // the accent of é, the dots of ö, once they are apart from their letter
        }
        if ((c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9') || c == u'_' || c == u'-') {
            result += c;
        } else if (const auto it = spelledOut.constFind(c); it != spelledOut.constEnd()) {
            result += *it;
        } else {
            result += u'_';
        }
    }
    // Remove consecutive underscores
    while (result.contains("__")) {
        result = result.replace("__", "_");
    }
    // Trim underscores from start and end
    while (result.startsWith('_')) {
        result = result.mid(1);
    }
    while (result.endsWith('_')) {
        result.chop(1);
    }
    if (result.isEmpty()) {
        // A name in a script without Latin letters (e.g. "游戏") leaves nothing: a stable id from its hash
        const bool hasLetters = std::any_of(input.cbegin(), input.cend(), [](QChar c) {
            return c.isLetterOrNumber();
        });
        result = hasLetters ? QStringLiteral("id_") + QString::fromLatin1(QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha1).toHex().left(12))
                            : QStringLiteral("empty_entity_id_detected_integration_error");
    }
    return result;
}

Entity::Entity(QObject *parent):
    QObject(parent)
{
    connect(TransportManager::mqttClient() , &QMqttClient::connected, this, &Entity::init);
}

QString Entity::hostname() const
{
    // the node id of every discovery topic: a host name like "desk.example.org" needs the same
    return sanitizeForMqttTopic(QHostInfo::localHostName());
}

QString Entity::baseTopic() const
{
    return topixPrefix() + "/" + hostname() + "/" + id();
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

void Entity::setName(const QString &newName)
{
    m_name = newName;
}

void Entity::setDiscoveryConfig(const QString &key, const QVariant &value)
{
    m_haConfig[key] = value;
}

void Entity::setHaIcon(const QString &newHaIcon)
{
    m_haIcon = newHaIcon;
    sendRegistration();
}

QString Entity::haIcon() const
{
    if (!m_haIcon.isEmpty()) {
        return m_haIcon;
    }

    return m_haConfig.value("icon").toString();
}

QString Entity::id() const
{
    if (m_id.isEmpty()) {
        qCWarning(base) << "Entity ID not set for entity" << name()
                   << " remember to use setId(IDstring)";
    }
    return sanitizeForMqttTopic(m_id);
}

void Entity::setId(const QString &newId)
{
    m_id = newId;
}

void Entity::init()
{}

void Entity::sendRegistration()
{
    if (haType().isEmpty()) {
        return;
    }
    QVariantMap config = m_haConfig;
    config["name"] = name();
    
    if (id() != "connected") { //special case
        config["availability_topic"] = topixPrefix() + "/" + hostname() + "/connected";
        config["payload_available"] = "on";
        config["payload_not_available"] = "off";
        const QString icon = haIcon();
        if (!icon.isEmpty()) {
            config["icon"] = icon;
        }
    }
    //Attributes topic
    config["json_attributes_topic"] = baseTopic() + "/attributes";
    if (!config.contains("device")) {
        config["device"] = QVariantMap({{"name", hostname()},
                                    {"identifiers", "linux_ha_bridge_" + hostname()},
                                    {"sw_version", QStringLiteral(PROJECT_VERSION)},
                                    {"manufacturer", QStringLiteral(PROJECT_DEVELOPERS)},
                                    {"model", QStringLiteral(PROJECT_NAME) },
                                    {"hw_version", QSysInfo::prettyProductName() + " - " + QSysInfo::kernelVersion()}});
    }
    config["unique_id"] = "linux_ha_control_"+ hostname() + "_" + id();
    TransportManager::mqttClient()->publish(discoveryPrefix() + "/" + haType() + "/" + hostname() + "/" + id() + "/config", QJsonDocument(QJsonObject::fromVariantMap(config)).toJson(QJsonDocument::Compact), 0, true);
    if (id() != "connected") { //special case
        TransportManager::mqttClient()->publish(topixPrefix() + "/" + hostname() + "/connected", "on", 0, false);
    }
}

//================ Code to allow runtime adding/removing of entities =======================//
void Entity::runtimeRegistration()
{
    if (TransportManager::mqttClient()->state() != QMqttClient::Connected) {
        return;
    }
    
    qCDebug(base) << "Runtime registration of entity:" << id() << "(" << name() << ")";
    init();
}

void Entity::unRegister()
{
    if (TransportManager::mqttClient()->state() != QMqttClient::Connected) {
        qCWarning(base) << "Cannot unregister entity" << id() << "(" << name() << ")"  << "- MQTT client not connected";
        return;
    }
    
    qCDebug(base) << "Unregistering entity:" << id() << "(" << name() << ")";
    TransportManager::mqttClient()->publish(discoveryPrefix() + "/" + haType() + "/" + hostname() + "/" + id() + "/config",
    QByteArray(), 0, true);
}

void Entity::setAttributes(const QVariantMap &attrs)
{
    m_attributes = attrs;
    publishAttributes();
}

QVariant Entity::convertForHomeAssistant(const QVariant &value) {
    switch (value.typeId()) {
        case QVariant::Bool:
            return value.toBool() ? "true" : "false";
        
        case QVariant::DateTime:
            return value.toDateTime().toString(Qt::ISODate);
        
        case QVariant::List: {
            QJsonArray jsonArray;
            for (const QVariant &item : value.toList()) {
                jsonArray.append(QJsonValue::fromVariant(item));
            }
            return jsonArray;
        }
        
        case QVariant::Map: {
            QJsonObject jsonObj;
            QVariantMap map = value.toMap();
            for (auto it = map.begin(); it != map.end(); ++it) {
                jsonObj[it.key()] = QJsonValue::fromVariant(it.value());
            }
            return jsonObj;
        }
        
        case QVariant::UserType: {
            return value;
        }
        
        default:
            return value;
    }
    return value;
}

void Entity::publishAttributes()
{
    if (TransportManager::mqttClient()->state() != QMqttClient::Connected)
        return;

    QJsonObject obj;
    for (auto it = m_attributes.constBegin(); it != m_attributes.constEnd(); ++it) {
        QVariant convertedValue = convertForHomeAssistant(it.value());
        obj[it.key()] = QJsonValue::fromVariant(convertedValue);
    }
    
    QJsonDocument doc(obj);
    TransportManager::mqttClient()->publish(baseTopic() + "/attributes", doc.toJson(QJsonDocument::Compact), 0, true);
}

} // namespace Entities
} // namespace KIOTShared