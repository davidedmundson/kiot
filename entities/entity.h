// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file entity.h
 * @brief Base Entity class declaration for KIOT Home Assistant integration
 * 
 * @details
 * This header defines the Entity base class which provides the common interface
 * for all KIOT entities that integrate with Home Assistant via MQTT.
 * 
 * The Entity class serves as the foundation for all entity types in the KIOT
 * system, providing core functionality for:
 * - MQTT topic management and discovery
 * - Entity identification and naming
 * - Attribute handling and publishing
 * - Home Assistant configuration and registration
 * 
 * All specific entity types (Sensor, Switch, Button, etc.) inherit from this
 * base class to ensure consistent behavior and integration patterns.
 */

#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

/**
 * @class Entity
 * @brief Base class for all KIOT Home Assistant entities
 * 
 * @details
 * The Entity class is the abstract base class for all entities in the KIOT
 * system. It defines the common interface and functionality required for
 * integrating with Home Assistant via MQTT.
 * 
 * Key responsibilities include:
 * - Managing entity identity (ID, name, icon)
 * - Handling MQTT topic generation and discovery
 * - Managing entity attributes (additional contextual data)
 * - Providing Home Assistant-compatible data conversion
 * - Coordinating with the MQTT client lifecycle
 * 
 * Note that entity state management is handled by derived classes, as
 * each entity type has its own unique state representation and behavior.
 * For example:
 * - BinarySensor manages boolean on/off state
 * - Sensor manages string or numeric state
 * - Switch manages toggleable state with commands
 * - Button manages momentary action triggers
 * 
 * @note All entities are QObject-derived to support Qt's signal/slot system
 *       and automatic memory management.
 */
class Entity: public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs an Entity
     * @param parent Parent QObject for memory management (optional)
     * 
     * @details
     * Initializes the entity with the given parent. The actual MQTT setup
     * is performed when the MQTT client connects, triggering the init() method.
     */
    Entity(QObject *parent);
    
    /**
     * @brief Sets the unique identifier for this entity
     * @param newId Unique identifier string
     * 
     * @details
     * Sets the entity's unique identifier used for MQTT topic construction
     * and Home Assistant discovery. The ID should be unique within the
     * KIOT instance to avoid topic conflicts.
     * 
     * @note The ID is combined with the system hostname to create globally
     *       unique MQTT topics.
     */
    void setId(const QString &newId);
    
    /**
     * @brief Gets the unique identifier for this entity
     * @return QString Entity identifier
     * 
     * @details
     * Returns the entity's unique identifier. If no ID has been set,
     * a warning is logged to help diagnose configuration issues.
     */
    QString id() const;

    /**
     * @brief Sets the display name for this entity
     * @param newName Display name for Home Assistant
     * 
     * @details
     * Sets the human-readable name that will be displayed in Home Assistant
     * for this entity. This name appears in the Home Assistant UI and
     * automation configuration.
     */
    void setName(const QString &newName);
    
    /**
     * @brief Gets the display name of this entity
     * @return QString Entity display name
     */
    QString name() const;

    /**
     * @brief Sets the Home Assistant icon for this entity
     * @param newHaIcon Material Design icon name (e.g., "mdi:lightbulb")
     * 
     * @details
     * Sets the icon that will be displayed for this entity in Home Assistant.
     * The icon name should follow Material Design Icons naming convention.
     * Setting the icon triggers re-registration with Home Assistant.
     */
    void setHaIcon(const QString &newHaIcon);
    
    /**
     * @brief Gets the Home Assistant icon for this entity
     * @return QString Material Design icon name
     * 
     * @details
     * Returns the icon configured for this entity. If no specific icon
     * has been set via setHaIcon(), it falls back to any icon configured
     * in the discovery configuration.
     */
    QString haIcon() const;

    /**
     * @brief Sets a discovery configuration parameter
     * @param key Configuration parameter name
     * @param value Configuration parameter value
     * 
     * @details
     * Adds or updates a configuration parameter for Home Assistant MQTT
     * discovery. These parameters are included in the discovery message
     * and control how the entity appears and behaves in Home Assistant.
     * 
     * Examples include:
     * - "unit_of_measurement": "%" for percentage sensors
     * - "device_class": "battery" for battery sensors
     * - "icon": "mdi:lightbulb" for icon configuration
     * - "command_topic": topic for entities accepting commands
     */
    void setDiscoveryConfig(const QString &key, const QVariant &value);
    
    /**
     * @brief Gets the system hostname for topic construction
     * @return QString Lowercase hostname of the system
     * 
     * @details
     * Returns the local system hostname converted to lowercase. This is
     * used as part of MQTT topic paths to ensure uniqueness across
     * multiple systems on the same network.
     */
    QString hostname() const;
    
    /**
     * @brief Constructs the base MQTT topic for this entity
     * @return QString Full MQTT topic path
     * 
     * @details
     * Returns the base MQTT topic for this entity in the format:
     * `[hostname]/[entity_id]`
     * 
     * This topic is used for the entity's primary state and serves as
     * the base for attribute and command topics.
     */
    QString baseTopic() const;
    
    /**
     * @brief Sets entity attributes for publishing to Home Assistant
     * @param attrs Map of attribute names to values
     * 
     * @details
     * Sets the entity's attributes and immediately publishes them to
     * Home Assistant. Attributes provide additional contextual information
     * about the entity's state beyond the primary value.
     * 
     * Examples of attributes:
     * - Battery: temperature, voltage, technology
     * - Camera: timestamp, image size, resolution
     * - Sensor: unit of measurement, accuracy
     * 
     * @note Attributes are separate from the entity's primary state, which
     *       is managed by derived entity classes.
     * @see publishAttributes() for the actual publishing implementation
     */
    void setAttributes(const QVariantMap &attrs);
    
    /**
     * @brief Gets the current entity attributes
     * @return QVariantMap Map of attribute names to values
     * 
     * @details
     * Returns the current set of attributes for this entity. Attributes
     * are additional data points that provide context about the entity's
     * state, such as battery temperature, device information, or timestamps.
     * 
     * @note This returns attributes only, not the entity's primary state.
     *       The primary state is managed and accessed through derived
     *       entity class methods.
     */
    QVariantMap attributes() { return m_attributes; }
    
protected:
    /**
     * @brief Initialization method called on MQTT connect
     * 
     * @details
     * This virtual method is called when the MQTT client connects to the
     * broker. It may be called multiple times if the connection is lost
     * and re-established.
     * 
     * Derived classes should override this method to perform entity-specific
     * initialization, such as:
     * - Setting the Home Assistant entity type with setHaType()
     * - Configuring discovery parameters with setDiscoveryConfig()
     * - Setting up MQTT subscriptions for command topics
     * - Performing initial state updates (entity-specific)
     * 
     * The base implementation is empty.
     * 
     * @note Derived classes must call sendRegistration() after configuring
     *       the entity type and discovery parameters.
     */
    virtual void init();
    
    /**
     * @brief Sends MQTT discovery registration to Home Assistant
     * 
     * @details
     * Publishes the entity's configuration to Home Assistant's MQTT
     * discovery topic, allowing Home Assistant to automatically create
     * and configure the entity.
     * 
     * This method should be called from init() after:
     * 1. Setting the entity type with setHaType()
     * 2. Configuring any discovery parameters with setDiscoveryConfig()
     * 
     * @note The entity type must be set before calling this method.
     */
    void sendRegistration();
    
    /**
     * @brief Sets the Home Assistant entity type
     * @param newHaType Home Assistant entity type string
     * 
     * @details
     * Sets the entity type for Home Assistant discovery. This determines
     * what type of entity is created in Home Assistant (e.g., "sensor",
     * "switch", "button", "camera").
     * 
     * Must be called before sendRegistration().
     * 
     * @note This is typically called in the derived class's init() method.
     */
    void setHaType(const QString &newHaType);
    
    /**
     * @brief Gets the Home Assistant entity type
     * @return QString Home Assistant entity type
     */
    QString haType() const;
    
    /**
     * @brief Publishes current attributes to Home Assistant
     * 
     * @details
     * Publishes the entity's current attributes to Home Assistant via
     * the MQTT attributes topic. This is called automatically by
     * setAttributes() but can also be called manually if needed.
     * 
     * Attributes are published to: `[hostname]/[entity_id]/attributes`
     * 
     * @note This method only publishes attributes, not the entity's
     *       primary state. State publishing is handled by derived classes.
     * @see setAttributes() for setting attribute values
     */
    void publishAttributes();
    
    /**
     * @brief Converts QVariant values to Home Assistant-compatible formats
     * @param value QVariant value to convert
     * @return QVariant Converted value suitable for Home Assistant
     * 
     * @details
     * Converts various QVariant types to formats that work well with
     * Home Assistant automations and templates. This ensures consistent
     * data formatting across all entities.
     * 
     * Supported conversions:
     * - bool → "true"/"false" strings (for template compatibility)
     * - QDateTime → ISO format strings (e.g., "2025-01-15T10:30:00Z")
     * - QVariantList → QJsonArray
     * - QVariantMap → QJsonObject
     * - Other types → passed through unchanged
     * 
     * This method is used internally by publishAttributes() to ensure
     * all attribute values are in Home Assistant-compatible formats.
     */
    QVariant convertForHomeAssistant(const QVariant &value);

private:
    /** @private Unique identifier for this entity */
    QString m_id;
    
    /** @private Display name for Home Assistant */
    QString m_name;
    
    /** @private Material Design icon name */
    QString m_haIcon = "";
    
    /** @private Home Assistant entity type */
    QString m_haType;
    
    /** @private Discovery configuration parameters */
    QVariantMap m_haConfig;
    
    /** @private Current entity attributes (additional contextual data) */
    QVariantMap m_attributes;
};
