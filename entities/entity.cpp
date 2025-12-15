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
#include "core.h"
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMqttClient>

/**
 * @brief Constructs an Entity
 * @param parent Parent QObject (optional)
 * 
 * @details
 * Initializes the entity and connects to the MQTT client's connected signal
 * to trigger initialization when the connection is established.
 */
Entity::Entity(QObject *parent):
    QObject(parent)
{
    connect(HaControl::mqttClient(), &QMqttClient::connected, this, &Entity::init);
}

/**
 * @brief Gets the system hostname for topic construction
 * @return QString Lowercase hostname of the system
 * 
 * @details
 * Returns the local hostname converted to lowercase, used as part of
 * MQTT topic paths to ensure uniqueness across multiple systems.
 */
QString Entity::hostname() const
{
    return QHostInfo::localHostName().toLower();
}

/**
 * @brief Constructs the base MQTT topic for this entity
 * @return QString Full MQTT topic path in format "hostname/entity_id"
 * 
 * @details
 * Combines the system hostname with the entity ID to create a unique
 * MQTT topic path for this entity's state and attributes.
 */
QString Entity::baseTopic() const
{
    return hostname() + "/" + id();
}

/**
 * @brief Gets the Home Assistant entity type
 * @return QString Home Assistant entity type (e.g., "sensor", "switch", "camera")
 * 
 * @details
 * Returns the Home Assistant entity type that determines how this entity
 * is represented and behaves in Home Assistant.
 */
QString Entity::haType() const
{
    return m_haType;
}

/**
 * @brief Sets the Home Assistant entity type
 * @param newHaType Home Assistant entity type string
 * 
 * @details
 * Sets the entity type for Home Assistant discovery. This determines
 * the type of entity created in Home Assistant (sensor, switch, etc.).
 */
void Entity::setHaType(const QString &newHaType)
{
    m_haType = newHaType;
}

/**
 * @brief Gets the display name of the entity
 * @return QString Entity display name
 */
QString Entity::name() const
{
    return m_name;
}

/**
 * @brief Sets the display name of the entity
 * @param newName New display name
 * 
 * @details
 * Sets the name that will be displayed in Home Assistant for this entity.
 * Also triggers re-registration with Home Assistant if the name changes.
 */
void Entity::setName(const QString &newName)
{
    m_name = newName;
}

/**
 * @brief Sets a discovery configuration parameter
 * @param key Configuration key
 * @param value Configuration value
 * 
 * @details
 * Adds or updates a configuration parameter for Home Assistant MQTT discovery.
 * These parameters are included in the discovery message sent to Home Assistant.
 */
void Entity::setDiscoveryConfig(const QString &key, const QVariant &value)
{
    m_haConfig[key] = value;
}


/**
 * @brief Sets the Home Assistant icon for this entity
 * @param newHaIcon Material Design icon name (e.g., "mdi:lightbulb")
 * 
 * @details
 * Sets the icon that will be displayed for this entity in Home Assistant.
 * Triggers re-registration with Home Assistant to update the icon.
 */
void Entity::setHaIcon(const QString &newHaIcon)
{
    m_haIcon = newHaIcon;
    sendRegistration();
}
/**
 * @brief Gets the Home Assistant icon for this entity
 * @return QString Material Design icon name
 */
QString Entity::haIcon() const
{
    if (!m_haIcon.isEmpty()) {
        return m_haIcon;
    }

    return m_haConfig.value("icon").toString();
}

/**
 * @brief Gets the unique identifier for this entity
 * @return QString Entity ID
 * 
 * @details
 * Returns the unique identifier for this entity within the KIOT instance.
 * Combined with hostname to create globally unique MQTT topics.
 * 
 */
 QString Entity::id() const
{
    if (m_id.isEmpty()) {
        qWarning() << "Entity ID not set for entity" << name()
                   << " remember to use setId(IDstring)";
    }
    return m_id;
}

/**
 * @brief Sets the unique identifier for this entity
 * @param newId New entity ID
 * 
 * @details
 * Sets the unique identifier for this entity. Should be unique within
 * the KIOT instance to avoid MQTT topic conflicts.
 */
void Entity::setId(const QString &newId)
{
    m_id = newId;
}

/**
 * @brief Default initialization method
 * 
 * @details
 * Empty default implementation. Derived classes should override this
 * to perform entity-specific initialization when the MQTT client connects.
 */
void Entity::init()
{}

/** @private Static discovery prefix for Home Assistant MQTT discovery 
 *  @note Should this be moved to the config file? to support custom prefixes
*/
static QString s_discoveryPrefix = "homeassistant";
/**
 * @brief Sends MQTT discovery registration to Home Assistant
 * 
 * @details
 * Publishes the entity's configuration to Home Assistant's MQTT discovery
 * topic, allowing Home Assistant to automatically create and configure
 * the entity.
 * 
 * The discovery message includes:
 * - Entity name and type
 * - Availability topic for connection state
 * - Attributes topic for additional data
 * - Device association for grouping entities
 * - Unique ID for entity identification
 * - Any custom discovery configuration set via setDiscoveryConfig()
 * 
 * @note The "connected" entity is treated specially and doesn't include
 *       availability configuration since it represents the connection state itself.
 */
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
        const QString icon = haIcon();
        if (!icon.isEmpty()) {
            config["icon"] = icon;
        }
    }
    //Attributes topic, since every mqtt entity looks like it supports attributes
    config["json_attributes_topic"] = baseTopic() + "/attributes";
    if (!config.contains("device")) {
        config["device"] = QVariantMap({{"identifiers", "linux_ha_bridge_" + hostname() }});
    }
    config["unique_id"] = "linux_ha_control_"+ hostname() + "_" + id();
    HaControl::mqttClient()->publish(s_discoveryPrefix + "/" + haType() + "/" + hostname() + "/" + id() + "/config", QJsonDocument(QJsonObject::fromVariantMap(config)).toJson(QJsonDocument::Compact), 0, true);
    if (id() != "connected") { //special case
        HaControl::mqttClient()->publish(hostname() + "/connected", "on", 0, false);
    }
}

/**
 * @brief Sets entity attributes for publishing to Home Assistant
 * @param attrs Map of attribute names to values
 * 
 * @details
 * Sets the entity's attributes and immediately publishes them to
 * Home Assistant via MQTT. Attributes provide additional data about
 * the entity's state beyond the primary value.
 * 
 * Attributes are automatically converted to Home Assistant-compatible
 * formats using convertForHomeAssistant() before publishing.
 */
void Entity::setAttributes(const QVariantMap &attrs)
{
    m_attributes = attrs;
    publishAttributes();
}

/**
 * @brief Converts QVariant values to Home Assistant-compatible formats
 * @param value QVariant value to convert
 * @return QVariant Converted value suitable for Home Assistant
 * 
 * @details
 * Converts various QVariant types to formats that work well with
 * Home Assistant automations and templates:
 * - bool → "true"/"false" strings
 * - QDateTime → ISO format strings
 * - QVariantList → QJsonArray
 * - QVariantMap → QJsonObject
 * - Other types → passed through unchanged
 * 
 * @todo Implement custom type converter for user-defined types if needed
 */
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

/**
 * @brief Publishes current entity attributes to Home Assistant via MQTT
 * 
 * @details
 * This method publishes the entity's current attributes to Home Assistant
 * through the MQTT attributes topic. Attributes provide additional contextual
 * information about the entity's state beyond the primary value.
 * 
 * The method performs the following steps:
 * 1. Checks if the MQTT client is connected (returns early if not)
 * 2. Converts all attributes to Home Assistant-compatible formats using
 *    convertForHomeAssistant()
 * 3. Serializes the attributes to JSON format
 * 4. Publishes the JSON data to the entity's attributes topic with
 *    retained flag set to true
 * 
 * The attributes are published to the topic pattern:
 * `[hostname]/[entity_id]/attributes`
 * 
 * Example published JSON:
 * @code
 * {
 *   "timestamp": "2025-01-15T10:30:00Z",
 *   "battery_level": 75,
 *   "charging": "true",
 *   "temperature": 25.5
 * }
 * @endcode
 * 
 * @note The method uses retained messages (flag=true) so Home Assistant
 *       receives the latest attributes even if it restarts or reconnects
 * @note Attributes are only published when the MQTT client is connected
 * @note The JSON is compact (no whitespace) to minimize bandwidth usage
 * @see setAttributes() for setting attribute values
 * @see convertForHomeAssistant() for attribute value conversion
 */
void Entity::publishAttributes()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    QJsonObject obj;
    for (auto it = m_attributes.constBegin(); it != m_attributes.constEnd(); ++it) {
        QVariant convertedValue = convertForHomeAssistant(it.value());
        obj[it.key()] = QJsonValue::fromVariant(convertedValue);
    }
    
    QJsonDocument doc(obj);
    HaControl::mqttClient()->publish(baseTopic() + "/attributes", doc.toJson(QJsonDocument::Compact), 0, true);
}