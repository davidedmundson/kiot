# Kiot - KDE Internet Of Things

## About

Kiot (KDE Internet Of Things) is a background daemon that exposes useful information and actions from your local desktop session to a home automation controller like Home Assistant.

**Important:** Kiot does not directly control smart home devices. Instead, it exposes desktop state information to your home automation controller, allowing you to create automations there. For example:
- If you want a light to turn on when your PC enters "Do Not Disturb" mode, Kiot exposes the "Do Not Disturb" state to Home Assistant, where you can create the automation to control the light.

## Current State

This is pre-alpha software suitable for users comfortable with:
- Compiling software from source
- Editing configuration files manually
- Testing early-stage software

## Setup

### Dependencies

Ensure you have these packages installed:
- `cmake`
- `extra-cmake-modules`
- `qt6-base` or `qt6-base-dev`
- `qt6-mqtt` or `qt6-mqtt-dev`
- `bluez-qt` or `libkf6bluezqt-dev`
- `pulseaudio-qt` or `libkf6pulseaudioqt-dev`

**Note:** Package names may vary slightly depending on your Linux distribution. If packages aren't available through your package manager, you may need to install them manually.

### Download and Install

1. Clone the repository:
   ```sh
   git clone https://github.com/davidedmundson/kiot.git
   cd kiot
   ```

2. Build and install:
   ```sh
   mkdir build
   cd build
   cmake ..
   make
   sudo make install  # May require sudo privileges
   ```

3. If you encounter missing dependencies during the build process, install them using your distribution's package manager.

## MQTT Configuration

Home Assistant must have MQTT server enabled. See the [Home Assistant MQTT documentation](https://www.home-assistant.io/integrations/mqtt/).

Create a configuration file at `~/.config/kiotrc` with the following content:

```ini
[general]
host=some.host
port=1883
user=myUsername
password=myPassword
```

> [!NOTE]
> If Kiot is running and you change the configuration, you must restart Kiot for changes to take effect.

### Home Assistant Managed MQTT
- `host`: Your Home Assistant local address
- `port`: 1883 (default)
- `user` and `password`: Credentials of a Home Assistant user (**recommended to create a dedicated MQTT user**)

### Home Assistant Container
- `host`: IP address where the MQTT broker is accessible
- `port`: 1883 (default)
- `user` and `password`: Credentials configured for your MQTT broker

On the Home Assistant side, everything should work automatically with MQTT discovery. After configuring Kiot, try rebooting Home Assistant, then launch the `kiot` program to see if everything connects properly.

## Project Goals

Compared to similar projects, Kiot focuses on practical desktop integration rather than exposing unnecessary system statistics. There's no value in exposing "kernel version" in a home automation context. Instead, Kiot emphasizes:

1. **Practical desktop integration** with features that are genuinely useful for home automation
2. **Plasma-specific properties** (while not exclusive to Plasma)
3. **Intuitive Home Assistant integration** with device triggers and actions that appear in an easy to use way

## Supported Features

### Stable Integrations

| Feature | Entity Type | Description |
|---------|-------------|-------------|
| User Activity | Binary Sensor | Detects when user is active/inactive |
| Locked State | Lock | Screen lock state monitoring and control |
| Power Control | Button | Suspend, hibernate, power off, and restart |
| Camera Activity | Binary Sensor | Detects when camera is in use |
| Accent Colour | Sensor | Current desktop accent color |
| Shortcuts | Device Trigger | Global keyboard shortcuts for HA automations |
| Night Mode | Binary Sensor | Night mode/blue light filter status |
| Active Window | Sensor | Currently focused application window |
| Audio Controller | Number + Select | Volume control and device selection |
| Battery Status | Sensor | Battery charge level and attributes |
| Do Not Disturb | Binary Sensor | DnD mode status |
| Gamepad Connected | Binary Sensor | Gamepad/joystick connection detection |
| Scripts | Button | Execute custom scripts |



## Configuration Examples

### Basic Configuration
```ini
[general]
host=192.168.1.100
port=1883
user=mqtt_user
password=secure_password
useSSL=false
```

### Scripts Configuration
```ini
[Scripts][launch_chrome]
Name=Launch Chrome
Exec=google-chrome

[Scripts][steam_bigpicture]
Exec=steam steam://open/bigpicture
Name=Launch steam bigpicture
```


### Shortcuts Configuration
```ini
[Shortcuts][myShortcut1]
Name=Do a thing
# Becomes available in KDE's Global Shortcuts KCM for key assignment
# Appears as a trigger in Home Assistant for keyboard-driven automations
```

### Integration Management
```ini
[Integrations]
# This section is auto-generated and lets you enable/disable integrations
AccentColour=true
Active=true
ActiveWindow=true
Audio=true
Battery=true
Bluetooth=true
CameraWatcher=true
DnD=true
Gamepad=true
LockedState=true
Nightmode=true
Notifications=true
PowerController=true
Scripts=true
Shortcuts=true
```

## Flatpak Build

Flatpak installation is also supported:

1. Clone this repository
2. Run:
   ```sh
   flatpak-builder build .flatpak-manifest.yaml --user --install --force-clean
   ```
   This builds and installs Kiot as a Flatpak, automatically fetching all dependencies.

### Flatpak Notes
- The Flatpak version does not autostart automatically
- Some integrations may have limited functionality due to Flatpak sandboxing

## Future Development

Long-term, Flatpak distribution is the primary focus. The goal is to publish to Flathub once a user interface is implemented.

### Planned Improvements
1. **Graphical Configuration UI** - Simplify setup without manual config file editing
2. **Enhanced Integration** - More desktop environment features and system monitoring
3. **Better Documentation** - Comprehensive guides and examples
4. **Extensibility / Plugin System** – Explore ways to allow community developed integrations


## Contributing

Contributions are welcome!
1. Test thoroughly
2. Document new integrations
3. Follow existing entity patterns

## Troubleshooting

### Common Issues
1. **MQTT Connection Failed**: Verify credentials and network connectivity
2. **Missing Entities in Home Assistant**: Check MQTT discovery is enabled in HA
3. **Permission Errors**: Some integrations may require additional permissions
4. **Flatpak Limitations**: Some system integrations may not work in sandboxed environment

### Getting Help
- Check the configuration examples above
- Review the Home Assistant MQTT documentation
- Examine system logs for error messages

---