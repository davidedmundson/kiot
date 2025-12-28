# KIOT Development Guide

## Navigation
- [Overview](#overview)
- [Architecture](#architecture)
  - [Core Components](#core-components)
  - [Integration Registration](#integration-registration)
  - [Entity System](#entity-system)
- [Creating New Integrations](#creating-new-integrations)
  - [Basic Structure](#basic-structure)
  - [Integration Template](#integration-template)
- [Configuration System](#configuration-system)
  - [Integration Configuration](#integration-configuration)
  - [Custom Configuration](#custom-configuration)
- [Entity Best Practices](#entity-best-practices)
- [Flatpak Considerations](#flatpak-considerations)
- [Testing Integrations](#testing-integrations)
- [Common Patterns](#common-patterns)
- [Contributing Guidelines](#contributing-guidelines)
- [See Also](#see-also)


## Overview


Kiot is built around the design of "plugins" living in the integration folder. These represent a semantic task, lockscreen, nightmode, etc.

Each integration in turn creates multiple entities. For example nightmode might expose the current brightness as a sensor and a switch for whether we're inhibited. Entities represent the entities you'd see in Home Assistant. Each entity can have IDs and names and icons. See the MQTT docs in Home Assistant.

Entity subclasses exist for BinarySensor, Sensor and so on, exposing convenience API.

The job of each Entity subclass is to abstract MQTT away from the logic code handling reconnects transparently from the logic code.

## Architecture

### Core Components

1. **HaControl** - Main controller class that manages MQTT connections and integration lifecycle
2. **Integrations** - Plugins that monitor system state and expose entities
3. **Entities** - Home Assistant-compatible representations of devices, sensors, and controls
4. **MQTT Client** - Handles communication with Home Assistant broker

### Integration Registration

Integrations are registered using the `REGISTER_INTEGRATION` macro:

```cpp
REGISTER_INTEGRATION("IntegrationName", setupFunction, enabledByDefault)
```

Where:
- `"IntegrationName"` - Unique identifier for the integration
- `setupFunction` - Function called when the integration should initialize
- `enabledByDefault` - Whether the integration is enabled by default in configuration

### Entity System

KIOT provides a comprehensive entity system with 1:1 mapping to Home Assistant entity types:

| Entity Class | Home Assistant Type | Description |
|--------------|---------------------|-------------|
| `BinarySensor` | `binary_sensor` | On/off state sensors |
| `Sensor` | `sensor` | Numeric or string state sensors |
| `Switch` | `switch` | Toggleable switches with commands |
| `Button` | `button` | Momentary action triggers |
| `Lock` | `lock` | Lock/unlock operations |
| `Event` | `device_trigger` | Automation triggers |
| `Select` | `select` | Option selection from predefined list |
| `Number` | `number` | Numeric input with constraints |
| `Text` | `text` | Text input for string values |
| `Camera` | `camera` | Image publishing |
| `Notify` | `notify` | Notification sending |
| `Update` | `update` | Firmware/software updates |
| `MediaPlayer` | `media_player` | Media playback control |

All entities inherit from the base `Entity` class which handles MQTT topic management, Home Assistant discovery, and common functionality.

## Creating New Integrations

### Basic Structure

A typical integration consists of:

1. **Setup function** - Called when integration initializes
2. **Watcher class** - Monitors system state and manages entities
3. **Entity instances** - Expose state to Home Assistant
4. **Configuration** - Optional config section in `~/.config/kiotrc`


### Integration Template

For a new integration, follow this template:

```cpp
// integrations/myintegration.cpp
#include "core.h"
#include "entities/[entity_type].h"  // e.g., sensor.h, switch.h
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(myintegration)
Q_LOGGING_CATEGORY(myintegration, "integration.MyIntegration")

class MyIntegrationWatcher : public QObject {
    Q_OBJECT
public:
    explicit MyIntegrationWatcher(QObject *parent = nullptr);
        : QObject(parent)
    {
        // Create entities
        m_entity = new EntityType(this);
        m_entity->setId("my_entity");
        m_entity->setName("My Entity");
    
        // Set up monitoring
        // Connect to system signals, timers, etc.
    }
private:
    // Your entity instances
    EntityType *m_entity;
    
private slots:
    void updateState() {
        // Update entity state based on system monitoring
        m_entity->setState(newState);
    
        // Optional: Set additional attributes
        QVariantMap attrs;
        attrs["attribute_name"] = attributeValue;
        m_entity->setAttributes(attrs);
    }
};

// Setup the integration
void setupMyIntegration() {
    new MyIntegrationWatcher(qApp);
}
// Register the integration
REGISTER_INTEGRATION("MyIntegration", setupMyIntegration, true)

#include "myintegration.moc"
```

## Configuration System

### Integration Configuration

Integrations can be enabled/disabled via the `[Integrations]` section in `~/.config/kiotrc`:

```ini
[Integrations]
Battery=true
Bluetooth=false
MyIntegration=true
```

### Custom Configuration

Integrations can define their own configuration sections:

```ini
[MyIntegration]
polling_interval=5000
threshold=75
```

Access configuration in your integration:

```cpp
KSharedConfigPtr config = KSharedConfig::openConfig("kiotrc");
KConfigGroup group = config->group("MyIntegration");
int interval = group.readEntry("polling_interval", 1000);
```

## Entity Best Practices

### 1. Naming Conventions
- Use lowercase with underscores: `battery_level`, `user_active`
- Keep IDs unique within the KIOT instance
- Use descriptive names in Home Assistant

### 2. State Management
- Update state only when meaningful changes occur
- Use appropriate data types (strings for Sensor, booleans for BinarySensor)
- Include relevant attributes for context

### 3. Error Handling
- Check for required dependencies
- Handle missing system features gracefully
- Log errors with context using `qCWarning()` or `qCCritical()`

### 4. Resource Management
- Clean up entities when devices are removed
- Use appropriate Qt parent-child relationships
- Unsubscribe from system signals when no longer needed

## Flatpak Considerations

When developing for Flatpak:

1. **Permissions** - Check the Flatpak manifest for required permissions
2. **DBus Access** - Some integrations require DBus portal permissions
3. **File System** - Use appropriate paths (e.g., `~/.config/` vs `/etc/`)
4. **Sandbox Limitations** - Some system APIs may not be available

Example Flatpak-specific code:

```cpp
#include <KSandbox>

if (KSandbox::isFlatpak()) {
    // Flatpak-specific implementation to make sure you get the correct file
    QString configPath = QDir::homePath() + "/.config/kdeglobals";
}
else {
    // Native implementation
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/kdeglobals";
}
```

## Testing Integrations

### Manual Testing
1. Build and install KIOT with your integration
2. Check that the integration appears in the KCM configuration module
3. Verify MQTT topics are created: `homeassistant/[type]/[hostname]/[entity_id]/config`
4. Test state updates in Home Assistant
5. Verify commands work (for Switch, Number, etc.)

### Debugging
- Enable debug logging: `QT_LOGGING_RULES="integration.*=true"`
- Monitor MQTT: `mosquitto_sub -t 'homeassistant/#' -v`
- Check system logs for errors

## Common Patterns

### 1. System Monitoring
```cpp
// Monitor DBus signals
QDBusConnection::sessionBus().connect(
    "org.kde.SomeService", "/SomePath", "org.kde.SomeInterface", "SignalName",
    this, SLOT(handleSignal(QDBusMessage)));
```

### 2. Periodic Updates
```cpp
QTimer *timer = new QTimer(this);
connect(timer, &QTimer::timeout, this, &MyIntegration::updateState);
timer->start(5000); // Update every 5 seconds
```

### 3. Device Discovery
```cpp
// Watch for device changes
connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded,
        this, &MyIntegration::deviceAdded);
```

## Contributing Guidelines

1. **Follow existing patterns** - Match the style of similar integrations
2. **Add documentation** - Update relevant README files
3. **Test thoroughly** - Verify with both native and Flatpak builds
4. **Consider permissions** - Ensure your integration works within sandbox constraints
5. **Update changelog** - Document new features in CHANGELOG.md


## See Also

- [entities/README.md](entities/README.md) - Complete entity reference
- [README.md](README.md) - User documentation and setup guide
- [CHANGELOG.md](CHANGELOG.md) - Version history and changes
- [Home Assistant MQTT Docs](https://www.home-assistant.io/integrations/mqtt/) - MQTT integration reference

---

*Note: This is a living document. As KIOT evolves, this development guide will be updated to reflect current best practices and architecture.*