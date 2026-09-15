# Kiot - Shared

## Navigation

- [About](#about)
- [Entities](#entities)
- [PlatformHelper](#platformhelper)
  - [Features](#platformhelper-features)
- [DBusProperty](#dbusproperty)
  - [Features](#dbusproperty-features)
- [See Also](#see-also)


## About

Welcome to the Shared folder! =)

This folder contains shared code used by both the core binary and the integrations. Here you will find everything you need to get started on your own custom integration.

The main purpose of this folder is to pave the way for an easier modular rewrite of the project in the future.

## Entities

For more information about the MQTT entity types supported by Kiot, please check out the [Entities README](entities/README.md).

## PlatformHelper

`PlatformHelper` is a centralized utility class, designed to abstract away platform-specific checks, environment detection, and sandbox escape handling. 

### Platformhelper Features

* **Platform & Architecture Detection:** Determines the current OS (`currentPlatform()`, `platformName()`) and CPU architecture (`currentArchitecture()`).
* **Sandbox & Confinement Checks:** Integrates with `KSandbox` to cleanly detect if the application runs inside a Flatpak (`isFlatpak()`) or Snap (`isSnap()`) environment, including sandbox permission validation (`checkFlatpakFeature()`).
* **Display & Desktop Environment:** Detects whether the session runs under Wayland (`isWayland()`) and identifies the active desktop environment (e.g., KDE, GNOME, Hyprland, Cosmic) via `detectDesktopEnvironment()`.
* **Host Process Spawning:** Simplifies running commands and processes on the host system from within a sandbox using `makeHostContext()` and `startHostProcess()`.
* **Standardized Logging:** Uses a unified logging macro (`DEFINE_LOGGER`) for clean and consistent output across the core and integrations.


## DBusProperty

`DBusProperty` is a lightweight helper class designed to monitor and fetch properties over D-Bus using the standard `org.freedesktop.DBus.Properties` interface.

### DBusProperty Features

* **Automatic Property Fetching:** Queries the initial value of a specified D-Bus property upon construction via a synchronous call.
* **Live Updates via Signals:** Automatically connects to `PropertiesChanged` signals on the session bus, updating the internal value and emitting `valueChanged()` whenever the property changes.


## See Also

- [KIOT Main](../README.md) for project overview and setup
- [KIOT Shared/Entities](/Shared/entities/README.md) for information about the shared lib entites part of the project
- [KIOT Integrations](../integrations/README.md) for creating new integrations
- [KIOT Example](../examples/README.md) for config examples and some inspiration
- [KIOT Helper Scripts](/scripts/README.md) for information about the helper scripts