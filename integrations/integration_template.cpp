// SPDX-FileCopyrightText: [YEAR] [YOUR NAME] <[YOUR EMAIL]>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file integration_template.cpp
 * @brief Template for creating new KIOT integrations
 * 
 * This template demonstrates the common patterns used in KIOT integrations.
 * Based on audio.cpp pattern - QObject-based class for complex integrations.
 */

#include "core.h"
#include "entities/entities.h"
#include <QCoreApplication>

// Include additional headers as needed for your integration
// #include <QDBusConnection>
// #include <QDBusInterface>
// #include <KNotification>
// #include "dbusproperty.h" // For DBusProperty helper
// etc.

// ============================================================================
// QObject-based Integration Class
// ============================================================================
// Use this pattern for integrations with complex state management,
// external resource monitoring, or multiple interconnected entities.
// Example: audio.cpp

class ExampleIntegration : public QObject
{
    Q_OBJECT
public:
    explicit ExampleIntegration(QObject *parent = nullptr);
    ~ExampleIntegration() override;

private slots:
    // Slot for handling Number value changes from Home Assistant
    void onNumberValueChanged(int newValue);
    
    // Slot for handling Select option changes from Home Assistant
    void onSelectOptionChanged(const QString &newOption);
    
    // Slot for handling Button triggers from Home Assistant
    void onButtonTriggered();
    
    // Slot for handling Switch state changes from Home Assistant
    void onSwitchStateChanged(bool newState);
    
    // Slot for monitoring external system changes
    void onExternalSystemUpdate();

private:
    // Helper methods
    void initializeResources();
    void cleanupResources();
    void updateAttributes();
    
    // Entity members - create in constructor, connect in initialization
    Number *m_number = nullptr;
    Select *m_select = nullptr;
    Button *m_button = nullptr;
    Switch *m_switch = nullptr;
    Sensor *m_sensor = nullptr;
    BinarySensor *m_binarySensor = nullptr;
    
    // Other state members
    QDBusInterface *m_dbusInterface = nullptr;
    QString m_currentState;
    bool m_isConnected = false;
};

// Constructor - Create entities and set up initial state
ExampleIntegration::ExampleIntegration(QObject *parent)
    : QObject(parent)
{
    // Create Number entity (like volume control in audio.cpp)
    m_number = new Number(this);
    m_number->setId("example_number");
    m_number->setName("Example Number");
    m_number->setDiscoveryConfig("icon", "mdi:gauge");
    m_number->setRange(0, 100, 1, "%"); // min, max, step, unit
    
    // Connect Number signal - triggered when HA changes the value
    connect(m_number, &Number::valueChangeRequested,
            this, &ExampleIntegration::onNumberValueChanged);
    
    // Create Select entity (like device selector in audio.cpp)
    m_select = new Select(this);
    m_select->setId("example_select");
    m_select->setName("Example Select");
    m_select->setDiscoveryConfig("icon", "mdi:format-list-bulleted");
    m_select->setOptions({"Option 1", "Option 2", "Option 3"});
    
    // Connect Select signal - triggered when HA selects an option
    connect(m_select, &Select::optionSelected,
            this, &ExampleIntegration::onSelectOptionChanged);
    
    // Create Button entity
    m_button = new Button(this);
    m_button->setId("example_button");
    m_button->setName("Example Button");
    m_button->setDiscoveryConfig("icon", "mdi:button-pointer");
    
    // Connect Button signal - triggered when HA presses the button
    connect(m_button, &Button::triggered,
            this, &ExampleIntegration::onButtonTriggered);
    
    // Create Switch entity
    m_switch = new Switch(this);
    m_switch->setId("example_switch");
    m_switch->setName("Example Switch");
    m_switch->setDiscoveryConfig("icon", "mdi:toggle-switch");
    
    // Connect Switch signal - triggered when HA toggles the switch
    connect(m_switch, &Switch::stateChangeRequested,
            this, &ExampleIntegration::onSwitchStateChanged);
    
    // Create Sensor entity (for string state)
    m_sensor = new Sensor(this);
    m_sensor->setId("example_sensor");
    m_sensor->setName("Example Sensor");
    m_sensor->setDiscoveryConfig("icon", "mdi:information");
    
    // Create BinarySensor entity (for boolean state)
    m_binarySensor = new BinarySensor(this);
    m_binarySensor->setId("example_binary");
    m_binarySensor->setName("Example Binary Sensor");
    m_binarySensor->setDiscoveryConfig("icon", "mdi:checkbox-marked-circle");
    
    // Initialize external resources (DBus, timers, etc.)
    initializeResources();
    
    // Set initial states
    m_number->setValue(50);
    m_select->setState("Option 1");
    m_switch->setState(false);
    m_sensor->setState("Initialized");
    m_binarySensor->setState(true);
}

// Destructor - Clean up resources
ExampleIntegration::~ExampleIntegration()
{
    cleanupResources();
}

// Initialize external resources
void ExampleIntegration::initializeResources()
{
    // Example: Initialize DBus connection
    m_dbusInterface = new QDBusInterface(
        "org.example.Service",
        "/org/example/Path",
        "org.example.Interface",
        QDBusConnection::sessionBus(),
        this
    );
    
    if (!m_dbusInterface->isValid()) {
        qWarning() << "Failed to connect to DBus service";
        m_sensor->setState("DBus Unavailable");
        return;
    }
    
    // Connect to DBus signals if needed
    // QDBusConnection::sessionBus().connect(...);
    
    m_isConnected = true;
    m_sensor->setState("Connected");
}

// Clean up resources
void ExampleIntegration::cleanupResources()
{
    // Clean up any allocated resources
    if (m_dbusInterface) {
        m_dbusInterface->deleteLater();
        m_dbusInterface = nullptr;
    }
}

// Handle Number value changes from Home Assistant
void ExampleIntegration::onNumberValueChanged(int newValue)
{
    qInfo() << "Number value changed to:" << newValue;
    
    // Apply the change to the actual system
    // Example: Set system volume, brightness, etc.
    
    // Update the entity state to reflect the change
    m_number->setValue(newValue);
    
    // Update attributes if needed
    updateAttributes();
}

// Handle Select option changes from Home Assistant
void ExampleIntegration::onSelectOptionChanged(const QString &newOption)
{
    qInfo() << "Select option changed to:" << newOption;
    
    // Apply the selection to the actual system
    // Example: Change audio output device, theme, etc.
    
    // Update the entity state to reflect the change
    m_select->setState(newOption);
}

// Handle Button triggers from Home Assistant
void ExampleIntegration::onButtonTriggered()
{
    qInfo() << "Button triggered";
    
    // Perform the button action
    // Example: Execute command, send notification, etc.
    
    // Buttons don't have state to update
}

// Handle Switch state changes from Home Assistant
void ExampleIntegration::onSwitchStateChanged(bool newState)
{
    qInfo() << "Switch state changed to:" << newState;
    
    // Apply the switch state to the actual system
    // Example: Enable/disable feature, start/stop service, etc.
    
    // Update the entity state to reflect the change
    m_switch->setState(newState);
}

// Handle external system updates
void ExampleIntegration::onExternalSystemUpdate()
{
    // Called when external system state changes
    // Example: DBus signal, timer timeout, etc.
    
    // Update entities to reflect current system state
    // m_number->setValue(currentValue);
    // m_binarySensor->setState(isActive);
    // m_sensor->setState(currentStatus);
    
    updateAttributes();
}

// Update entity attributes
void ExampleIntegration::updateAttributes()
{
    QVariantMap attributes;
    attributes["connected"] = m_isConnected;
    attributes["timestamp"] = QDateTime::currentDateTime().toString();
    
    // Set attributes on relevant entities
    if (m_sensor) {
        m_sensor->setAttributes(attributes);
    }
}

// ============================================================================
// Setup Function and Registration
// ============================================================================

void setupExampleIntegration()
{
    new ExampleIntegration(qApp);
}

// Every integration MUST end with this macro call.
// Parameters:
// 1. Integration name (string) - appears in logs and configuration
// 2. Setup function name (function pointer)
// 3. Enabled by default (bool) - usually true

REGISTER_INTEGRATION("ExampleIntegration", setupExampleIntegration, true)

// Required for QObject-based classes with signals/slots
#include "integration_template.moc"