# KIOT Integrations

This directory contains integrations that extend KIOT's functionality. Each integration
provides one or more entities (sensors, buttons, etc.) that can be monitored or controlled
through Home Assistant.

## Quick Navigation

- [Overview](#overview)
- [Creating a New Integration](#creating-a-new-integration)
  - [Template Setup](#1-start-with-the-template)
  - [Add to CMakeLists.txt](#2-add-to-cmakeliststxt)
  - [QObject-based Class Structure](#3-understanding-the-pattern-based-on-audiocpp)
- [Entity Types & Usage](#4-entity-types-and-their-usage)
- [Key Principles from audio.cpp](#5-key-principles-from-audiocpp)
- [Common Integration Patterns](#6-common-integration-patterns)
- [Existing Integrations Reference](#7-existing-integrations-reference)
- [Best Practices](#8-best-practices)
- [Testing Your Integration](#9-testing-your-integration)
- [Common Issues](#10-common-issues)
- [Example Implementation](#example-implementation)

---

## Overview

Integrations are C++ source files that are compiled into the KIOT binary. The primary
pattern used is **QObject-based classes** for complex integrations with state management,
external resource monitoring, or multiple interconnected entities.

---

## Creating a New Integration

### 1. Start with the Template

Use [Integration template](./integration_template.cpp)  as a starting point. It follows the same pattern as `audio.cpp`:

```bash
cp integration_template.cpp my_integration.cpp
```

### 2. Add to CMakeLists.txt

Add your new integration to the `KIOT_INTEGRATIONS_SRC` list in `CMakeLists.txt`:

```cmake
set(KIOT_INTEGRATIONS_SRC
    # ... existing files ...
    my_integration.cpp
)
```

### 3. Understanding the Pattern (Based on audio.cpp)

#### QObject-based Class Structure

```cpp
class MyIntegration : public QObject
{
    Q_OBJECT
public:
    explicit MyIntegration(QObject *parent = nullptr);
    ~MyIntegration() override;
    
private slots:
    // Handlers for HA interactions
    void onNumberValueChanged(int newValue);
    void onSelectOptionChanged(const QString &newOption);
    
    // Handlers for external system updates
    void onExternalSystemUpdate();
    
private:
    // Entity members
    Number *m_number = nullptr;
    Select *m_select = nullptr;
    // ... other entities
    
    // State and resource members
    QDBusInterface *m_dbusInterface = nullptr;
};
```

#### Key Components:
1. **Constructor**: Create entities and connect their signals
2. **Destructor**: Clean up external resources
3. **Signal Handlers**: Respond to Home Assistant changes
4. **Entity Updates**: Update entities when system state changes

---

## 4. Entity Types and Their Usage

#### Number (for numeric values)
```cpp
Number *volume = new Number(this);
volume->setId("output_volume");
volume->setName("Output Volume");
volume->setRange(0, 100, 1, "%"); // min, max, step, unit
connect(volume, &Number::valueChangeRequested,
        this, &MyIntegration::onVolumeChanged);

// When HA changes the value:
void MyIntegration::onVolumeChanged(int newValue) {
    // Apply to system
    applyVolumeToSystem(newValue);
    // Update entity state
    volume->setValue(newValue);
}
```

#### Select (for dropdown selections)
```cpp
Select *deviceSelector = new Select(this);
deviceSelector->setId("output_device");
deviceSelector->setName("Output Device");
deviceSelector->setOptions({"Headphones", "Speakers", "HDMI"});
connect(deviceSelector, &Select::optionSelected,
        this, &MyIntegration::onDeviceSelected);

// When HA selects an option:
void MyIntegration::onDeviceSelected(const QString &device) {
    // Switch system to selected device
    switchToDevice(device);
    // Update entity state
    deviceSelector->setState(device);
}
```

#### Button (for trigger actions)
```cpp
Button *actionButton = new Button(this);
actionButton->setId("perform_action");
actionButton->setName("Perform Action");
connect(actionButton, &Button::triggered,
        this, &MyIntegration::onActionTriggered);

// When HA triggers the button:
void MyIntegration::onActionTriggered() {
    // Perform the action
    performSystemAction();
}
```

#### Switch (for toggle states)
```cpp
Switch *featureSwitch = new Switch(this);
featureSwitch->setId("enable_feature");
featureSwitch->setName("Enable Feature");
connect(featureSwitch, &Switch::stateChangeRequested,
        this, &MyIntegration::onFeatureToggled);

// When HA toggles the switch:
void MyIntegration::onFeatureToggled(bool enabled) {
    // Enable/disable feature in system
    setFeatureEnabled(enabled);
    // Update entity state
    featureSwitch->setState(enabled);
}
```

#### Sensor (for string states)
```cpp
Sensor *statusSensor = new Sensor(this);
statusSensor->setId("system_status");
statusSensor->setName("System Status");
// Update when system state changes:
statusSensor->setState("Running");
statusSensor->setAttributes({{"uptime", "5 hours"}});
```

#### BinarySensor (for boolean states)
```cpp
BinarySensor *activitySensor = new BinarySensor(this);
activitySensor->setId("user_active");
activitySensor->setName("User Active");
// Update when activity changes:
activitySensor->setState(true);
```

---

## 5. Key Principles from audio.cpp

#### Bidirectional State Sync
- **HA → System**: Listen to entity signals (`valueChangeRequested`, `optionSelected`, etc.)
- **System → HA**: Call entity methods (`setValue()`, `setState()`, etc.) when system changes

#### Resource Management
- Initialize external resources in constructor
- Clean up in destructor
- Handle connection/disconnection gracefully

#### Error Handling
- Check DBus interface validity
- Log warnings for recoverable errors
- Set entity states to reflect availability

---

## 6. Common Integration Patterns

#### DBus Monitoring (like audio.cpp)
```cpp
// Connect to DBus signals
connect(m_ctx, &PulseAudioQt::Context::sinkAdded,
        this, &MyIntegration::updateDevices);
connect(m_ctx, &PulseAudioQt::Context::sinkRemoved,
        this, &MyIntegration::updateDevices);

// Update entities when DBus signals arrive
void MyIntegration::updateDevices() {
    // Refresh device lists
    // Update Select options
    // Update current states
}
```

#### Timer-based Updates
```cpp
QTimer *updateTimer = new QTimer(this);
connect(updateTimer, &QTimer::timeout,
        this, &MyIntegration::refreshState);
updateTimer->start(5000); // Update every 5 seconds
```

#### Configuration Reading
```cpp
auto config = KSharedConfig::openConfig()->group("MyIntegration");
QString setting = config.readEntry("Setting", "default");
```

---

## 7. Existing Integrations Reference

#### Complex QObject-based Integrations (Primary Pattern)
- [Audio volume and device control](./audio.cpp) - (reference implementation)
- [Active window tracking](./activewindow.cpp) - with KWin script
- [Night mode control](./nightmode.cpp) - with DBus inhibition

#### Simpler Patterns
- [DnD State](./dndstate.cpp) - DBus property monitoring
- [User Activity](./active.cpp) - sensor with timer
- [Power Control](./suspend.cpp) - buttons for power control

---

## 8. Best Practices

1. **Follow audio.cpp Pattern**: Use QObject-based classes for complex integrations
2. **Bidirectional Sync**: Always update entity state after system changes
3. **Resource Cleanup**: Implement destructor for resource management
4. **Error Logging**: Use appropriate qWarning/qInfo/qDebug levels
5. **State Initialization**: Set initial entity states in constructor
6. **Signal Disconnection**: Disconnect signals in destructor if needed

---

## 9. Testing Your Integration

1. Add to CMakeLists.txt and rebuild
2. Check KIOT logs for initialization messages
3. Verify entities appear in Home Assistant
4. Test both directions:
   - HA → System: Change values in HA, verify system updates
   - System → HA: Change system state, verify HA updates

---

## 10. Common Issues

- **Missing .moc file**: QObject classes need `#include "filename.moc"`
- **No REGISTER_INTEGRATION**: Every integration must register itself
- **State not syncing**: Remember to call `setValue()`/`setState()` after system changes
- **DBus issues**: Verify service names and object paths

---

## Example Implementation

See [Integration template](./integration_template.cpp) for a complete example following the `audio.cpp` pattern.
It demonstrates all entity types and proper bidirectional state management.

## See Also

- [KIOT Main README](../README.md) for project overview and setup
- [KIOT Entities README](../entities/README.md) for available entities to use with integrations