# KIOT Entities

This directory contains the core entity implementations for the KIOT (KDE Internet Of Things) project. Entities are the fundamental building blocks that represent devices, sensors, and controls in Home Assistant via MQTT.

## Quick Navigation

- [Overview](#overview)
- [Main Entry Point](#main-entry-point)
- [Available Entity Types](#available-entity-types)
  - [Binary Sensor](#1-binary-sensor)
  - [Sensor](#2-sensor)
  - [Switch](#3-switch)
  - [Button](#4-button)
  - [Lock](#5-lock)
  - [Event](#6-event)
  - [Select](#7-select)
  - [Number](#8-number)
  - [Textbox](#9-textbox)
- [Creating New Entities](#creating-new-entities)
- [MQTT Topic Structure](#mqtt-topic-structure)
- [Home Assistant Discovery](#home-assistant-discovery)
- [Best Practices](#best-practices)
- [Integration with Home Assistant](#integration-with-home-assistant)
- [Testing Entities](#testing-entities)
- [Contributing New Entities](#contributing-new-entities)
- [See Also](#see-also)

---

## Overview

Entities in KIOT are QObject-based classes that extend the `Entity` base class. Each entity type corresponds to a specific Home Assistant MQTT entity type and provides the necessary MQTT discovery configuration and communication logic.

---

## Main Entry Point: `entities.h`

### The Unified Header

The `entities.h` header file is the primary interface that all integrations should use. It provides a single include point for accessing all available entity types.

**Usage in Integrations:**
```cpp
#include "entities/entities.h"
```

This single include gives access to:
- All entity class declarations
- The Entity base class
- Any helper functions or types related to entities

**Why use `entities.h` instead of individual headers?**
1. **Simplicity**: One include instead of multiple
2. **Maintainability**: Centralized header management
3. **Consistency**: Ensures all integrations use the same interface
4. **Future-proofing**: Easy to add new entities without changing integration code

---

## Available Entity Types

KIOT entity types are designed as a strict 1:1 mapping to Home Assistant entity types, with no additional abstraction layer in between.

### 1. **Binary Sensor** (`binarysensor.h` / `binarysensor.cpp`)
Represents binary (on/off) state sensors.

**Home Assistant Type:** `binary_sensor`  
**Use Cases:** User activity, camera in use, night mode status, gamepad connected, DnD state

**Example Configuration:**
```cpp
BinarySensor *sensor = new BinarySensor(parent);
sensor->setId("user_active");
sensor->setName("User Active");
sensor->setState(true); // or false
```

### 2. **Sensor** (`sensor.h` / `sensor.cpp`)
Represents numeric or string state sensors.

**Home Assistant Type:** `sensor`  
**Use Cases:** Battery percentage, accent color, active window title, temperature readings

**Example Configuration:**
```cpp
Sensor *sensor = new Sensor(parent);
sensor->setId("battery_level");
sensor->setName("Battery Level");
sensor->setState("75"); // String representation
sensor->setDiscoveryConfig("unit_of_measurement", "%");
```

### 3. **Switch** (`switch.h` / `switch.cpp`)
Represents binary switches that can be toggled.

**Home Assistant Type:** `switch`  
**Use Cases:** Bluetooth adapter control, Docker container start/stop, SystemD service control

**Example Configuration:**
```cpp
Switch *sw = new Switch(parent);
sw->setId("bluetooth_adapter");
sw->setName("Bluetooth Adapter");
sw->setState(true); // Current state
connect(sw, &Switch::stateChangeRequested, [](bool newState) {
    // Handle state change
});
```

### 4. **Button** (`button.h` / `button.cpp`)
Represents momentary buttons that trigger actions.

**Home Assistant Type:** `button`  
**Use Cases:** Script execution, power controls (suspend, restart), custom actions

**Example Configuration:**
```cpp
Button *btn = new Button(parent);
btn->setId("suspend");
btn->setName("Suspend");
connect(btn, &Button::triggered, []() {
    // Perform suspend action
});
```

### 5. **Lock** (`lock.h` / `lock.cpp`)
Represents lock entities with lock/unlock capabilities.

**Home Assistant Type:** `lock`  
**Use Cases:** Screen lock state monitoring and control

**Example Configuration:**
```cpp
Lock *lock = new Lock(parent);
lock->setId("screen_lock");
lock->setName("Screen Lock");
lock->setState(true); // Locked state
connect(lock, &Lock::stateChangeRequested, [](bool lockState) {
    // Lock or unlock the screen
});
```

### 6. **Event** (`event.h` / `event.cpp`)
Represents event triggers for Home Assistant automations.

**Home Assistant Type:** `device_trigger` (via MQTT discovery)  
**Use Cases:** Global keyboard shortcuts, system events, custom triggers

**Example Configuration:**
```cpp
Event *event = new Event(parent);
event->setId("media_play_pause");
event->setName("Media Play/Pause");
// Trigger from code:
event->trigger();
```

### 7. **Select** (`select.h` / `select.cpp`)
Represents selection entities with predefined options.

**Home Assistant Type:** `select`  
**Use Cases:** Audio output device selection, theme selection, mode switching

**Example Configuration:**
```cpp
Select *select = new Select(parent);
select->setId("audio_output");
select->setName("Audio Output");
select->setOptions({"Headphones", "Speakers", "HDMI"});
select->setState("Headphones");
connect(select, &Select::stateChangeRequested, [](const QString &option) {
    // Switch audio output
});
```

### 8. **Number** (`number.h` / `number.cpp`)
Represents numeric input entities with range constraints.

**Home Assistant Type:** `number`  
**Use Cases:** Volume control, brightness adjustment, numeric parameters

**Example Configuration:**
```cpp
Number *number = new Number(parent);
number->setId("volume");
number->setName("Volume");
number->setRange(0, 100);
number->setStep(5);
number->setState(75);
connect(number, &Number::stateChangeRequested, [](double value) {
    // Set volume level
});
```

### 9. **Textbox** (`textbox.h` / `textbox.cpp`)
Represents text input entities for string values.

**Home Assistant Type:** `text`  
**Use Cases:** Script argument input, custom command entry, text-based configuration

**Example Configuration:**
```cpp
Textbox *textbox = new Textbox(parent);
textbox->setId("script_argument");
textbox->setName("Script Argument");
textbox->setState("default_value");
connect(textbox, &Textbox::stateChangeRequested, [](const QString &text) {
    // Use text as argument for script execution
    executeScriptWithArgument(text);
});
```

---

## Creating New Entities

### Basic Entity Creation Template

```cpp
// myentity.h
#pragma once
#include "entity.h"

class MyEntity : public Entity
{
    Q_OBJECT
public:
    MyEntity(QObject *parent = nullptr);
    
    // Custom methods
    void setCustomValue(const QString &value);
    
Q_SIGNALS:
    void customSignal(const QString &data);
    
protected:
    void init() override;
    
private:
    QString m_customData;
};

// myentity.cpp
#include "myentity.h"
#include "core.h"
#include <QMqttClient>

MyEntity::MyEntity(QObject *parent)
    : Entity(parent)
{
}

void MyEntity::init()
{
    setHaType("sensor"); // or appropriate HA type
    setDiscoveryConfig("unit_of_measurement", "units");
    sendRegistration();
}

void MyEntity::setCustomValue(const QString &value)
{
    m_customData = value;
    setState(value);
    QVariantMap attrs;
    attrs["custom_attribute"] = m_customData;
    setAttributes(attrs);
}
```

### Key Steps for New Entities:
1. Inherit from `Entity` base class
2. Override `init()` for MQTT configuration
3. Call `setHaType()` with appropriate Home Assistant type
4. Use `setDiscoveryConfig()` for entity-specific settings
5. Call `sendRegistration()` to register with Home Assistant
6. Use `setState()` and `setAttributes()` to update entity state

---

## MQTT Topic Structure

All entities follow this topic pattern:

```
[hostname]/[entity_id]              # State topic
[hostname]/[entity_id]/attributes   # Attributes topic
[hostname]/[entity_id]/command      # Command topic (if applicable)
```

**Example:** For hostname "my-pc" and entity ID "battery":
- State: `my-pc/battery`
- Attributes: `my-pc/battery/attributes`
- Command: `my-pc/battery/command` (for entities supporting commands)

---

## Home Assistant Discovery

Entities automatically register with Home Assistant using MQTT discovery with this topic:

```
homeassistant/[ha_type]/[hostname]/[entity_id]/config
```

**Example Discovery Message:**
```json
{
  "name": "Battery Level",
  "state_topic": "my-pc/battery",
  "json_attributes_topic": "my-pc/battery/attributes",
  "availability_topic": "my-pc/connected",
  "payload_available": "on",
  "payload_not_available": "off",
  "unit_of_measurement": "%",
  "device": {
    "identifiers": ["linux_ha_bridge_my-pc"]
  },
  "unique_id": "linux_ha_control_my-pc_battery"
}
```

---

## Best Practices

### 1. **Entity Naming**
- Use descriptive, lowercase IDs with underscores: `battery_level`, `user_active`
- Keep names concise but clear: "Battery Level", "User Active"
- Avoid special characters in IDs

### 2. **State Management**
- Update state only when meaningful changes occur
- Use appropriate data types for state values
- Include relevant attributes for additional context

### 3. **Error Handling**
- Check MQTT connection before publishing
- Handle missing dependencies gracefully
- Log errors with context for debugging

---

## Integration with Home Assistant

Each entity type maps directly to Home Assistant entity types:

| KIOT Entity | Home Assistant Type | Key Features |
|-------------|---------------------|--------------|
| BinarySensor | binary_sensor | on/off state, device class support |
| Sensor | sensor | numeric/string values, units |
| Switch | switch | toggleable state, command support |
| Button | button | momentary action trigger |
| Lock | lock | lock/unlock operations |
| Event | device_trigger | automation triggers |
| Select | select | option selection |
| Number | number | numeric input with constraints |
| Textbox | text | textbox for input  |

---

## Testing Entities

### Manual Testing
1. Start KIOT with the entity integration enabled
2. Check MQTT topics are created correctly
3. Verify Home Assistant discovers the entity
4. Test state updates and commands

### Debug Tips
- Enable debug logging in KIOT
- Monitor MQTT traffic with `mosquitto_sub`
- Check Home Assistant logs for discovery issues
- Verify topic permissions and connectivity

---

## Contributing New Entities

When adding new entity types:

1. Follow existing naming and coding conventions
2. Document the entity's purpose and usage
3. Include example configurations
4. Test with Home Assistant integration
5. Update this README with new entity information

---

## See Also

- [Home Assistant MQTT Integration Documentation](https://www.home-assistant.io/integrations/mqtt/)
- [Home Assistant Entity Types](https://www.home-assistant.io/integrations/#search/mqtt)
- [KIOT Main README](../README.md) for project overview and setup
- [KIOT Integrations README](../integrations/README.md) for creating new integrations