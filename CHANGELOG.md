# Changelog

All notable changes to this experimental branch of Kiot will be documented in this file.

## [Unreleased]

### New Features

#### Entities
- **Camera Entity**: Added support for MQTT camera entity with image publishing capabilities
- **Notify Entity**: Added notification entity for sending notifications via Home Assistant
- **Update Entity**: Added update entity for tracking and managing software updates
- **MediaPlayer Entity**: Added media player entity for audio/video playback control (requires custom integration)
- **Event Entity**: Enhanced to fully support Home Assistant MQTT device trigger integration
  - Added support for all standard trigger types: button_short_press, button_short_release, button_long_press, button_long_release, button_double_press, button_triple_press, button_quadruple_press, button_quintuple_press
  - Added support for standard subtypes: turn_on, turn_off, button_1 through button_6
  - Added `triggerWithPayload()` method for custom trigger types (enables future extensions like file/directory watchers, system events, etc.)
  - Maintains full backward compatibility with existing configurations

#### Integrations
- **Docker Integration**: Expose selected Docker containers as switches with attributes showing image name, status, and container ID
- **SystemD Integration**: Expose selected systemd user services as switches for start/stop control
- **MPRIS Integration**: Expose MPRIS media players over MQTT, automatically tracks currently active player (requires custom integration)
- **GameLauncher Integration**: Detect installed Steam/Heroic/lutris games and expose them as a dropdown menu in Home Assistant for voice-assisted game launching and such
- **Flatpak Updater Integration** (Flatpak only): Check for latest releases on GitHub and enable automatic installation via user-installed Flatpak setup
- **ActiveWindow Integration**: Updated to work in flatpak
- **Scripts Integration**: Added support for custom input variables, if exec line contains "{arg}", it exposes a textbox in HA and replaces {arg} with the input value
- **Shortcuts Integration**: Enhanced to expose system shortcuts via KGlobalAccel DBus interface
  - Automatically discovers all available system shortcuts from KDE components (kwin, krunner, plasmashell, etc.)
  - Exposes shortcuts as a select entity dropdown in Home Assistant
  - Allows triggering any system shortcut directly from Home Assistant
  - Automatically returns to "Default" state after execution
  - Works in both native and Flatpak installations
  - Eliminates need for external tools like ydotool for shortcut automation
- **AccentColour Integration**: Completely rewritten and fixed
  - Now correctly reads KDE accent color from kdeglobals config file
  - Properly handles both native and Flatpak environments
  - Exposes color in multiple formats: hex (#RRGGBB), RGB tuple, and individual RGB components
  - Includes attributes for source (wallpaper/custom/theme), last used color, and wallpaper detection flag
  - Real-time updates when accent color changes in KDE System Settings


#### User Interface
- **KCM Improvements**: 
  - Dynamic tabs based on config file content (Uses a dropdown menu when window width is too low to show all tabs)
  - Support for creating/deleting scripts and shortcuts directly from UI
  - Auto-restart Kiot after pressing OK/Apply to ensure new configurations are used
  - Improved tab navigation with adaptive layout (dropdown for narrow windows, tabs for wide windows)
- **System Tray Icon**: 
  - Green/yellow/red icon based on MQTT client connection state
  - Menu for opening settings, config file, restarting and reconnecting

#### Core Improvements
- **Auto-start Management**: Completely redesigned auto-start system
  - Replaced .desktop file autostart with systemd user services
  - Universal solution that works for both native and Flatpak installations
  - Added "Autostart" toggle in General settings (autostart=true/false in config)
  - Uses D-Bus API for service management
  - Automatic validation and synchronization between config and service state
- **Service Manager**: Added ServiceManager class for handling systemd user service lifecycle
- **Version Management**: Updated CMakeLists to make project version available throughout codebase
- **Year Display**: Changed KAboutData from static "2024" to dynamic "2024-CurrentYear"
- **Config Validation**: Added config validator that opens KCM module and notifies user if configuration has issues
- **Auto-config Creation**: Basic config file creation and KCM module opening when required keys are missing

#### Entity Categories
- **Standardized entity categorization**: Added some  entity_category configuration to make home assistant easier to navigate
  - Diagnostic category: Battery, Update, ConnectedNode, 
  - Improves Home Assistant UI organization and user experience

#### Helper Scripts
- **Main Helper Menu** (`helper.sh`): Interactive menu for easy installation and setup
- **Dependency Installer** (`scripts/dependencies.sh`): Automatic dependency installation for multiple distributions (apt/pacman)
- **Native Installer** (`scripts/native.sh`): Interactive menu for native build and installation
- **Flatpak Installer** (`scripts/flatpak.sh`): Interactive menu for Flatpak build and installation

### Build System
- **Flatpak Support**: Added complete Flatpak build system with dedicated folder
- **CMake Options**: Added `BUILD_FLATPAK` option for conditional inclusion of Flatpak-only content
- **DBus Interfaces**: Automatic generation of DBus interface files during build
- **CI/CD**: Added auto-release pipeline for Flatpak builds

### Fixes and Improvements
- **Code Organization**: Removed monolithic entities.h and updated each file to include relevant headers
- **Gitignore**: Updated to exclude build artifacts and temporary files
- **Flatpak Manifest**: Updated to provide necessary permissions for all integrations
- **UI Responsiveness**: Improved window resizing behavior and tab navigation


### Documentation
- Updated README.md with new integrations and features
- Added comprehensive documentation for helper scripts
- Added this changelog file for tracking changes

## Technical Details

### Auto-start Implementation
- **Service Files**: Dynamically generated systemd user service files
- **Path Handling**: Automatic detection of Flatpak vs native environment
- **D-Bus Integration**: Uses org.freedesktop.systemd1 D-Bus API for service management
- **Cross-platform**: Works identically on native and Flatpak installations

### Shortcuts Integration Implementation
- **Registration**: kept the registerShortcuts function as it was, but added a new function to handle the shortcuts
- **KGlobalAccel DBus Integration**: Uses org.kde.kglobalaccel DBus interface to discover and trigger system shortcuts
- **Component Discovery**: Automatically detects available KDE components (kwin, krunner, plasmashell, etc.)
- **Fallback Mechanism**: Tries multiple components if primary component fails
- **Select Entity**: Exposes shortcuts as a dropdown menu with "Default" as first option
- **Automatic Reset**: Returns to "Default" state after shortcut execution
- **Cross-platform**: Works in both native and Flatpak 

### AccentColour Integration Implementation
- **Config File Handling**: Correctly reads from ~/.config/kdeglobals instead of kiotrc
- **Cross-platform Support**: Different file watching strategies for native (KConfigWatcher) and Flatpak (QFileSystemWatcher)
- **Atomic Write Handling**: Properly handles KDE's atomic file replacement with remove/add file watching pattern
- **Multiple Formats**: Exposes color in hex (#RRGGBB), RGB tuple (r,g,b), and individual RGB components
- **Metadata Attributes**: Includes source (wallpaper/custom/theme), last used color, and wallpaper detection
- **Real-time Updates**: Monitors config file changes and updates Home Assistant immediately
- **Fallback Values**: Provides KDE default blue (#3DAEE9) when no custom accent is set


## Notes
- This branch is highly experimental and intended for testing purposes
- Some features (like MediaPlayer and MPRIS) require custom Home Assistant integrations
- Helper scripts provide simplified installation for new users
- Auto-start now uses systemd user services which require systemd to be running (standard on most modern Linux distributions)
- Shortcuts integration eliminates dependency on external automation tools like ydotool for KDE shortcut automation
- AccentColour integration now provides useful color data for Home Assistant automations (e.g., sync lights with desktop accent color)

---

