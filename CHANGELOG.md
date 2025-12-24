# Changelog

All notable changes to this experimental branch of Kiot will be documented in this file.

## [Unreleased]

### New Features

#### Entities
- **Camera Entity**: Added support for MQTT camera entity with image publishing capabilities
- **Notify Entity**: Added notification entity for sending notifications via Home Assistant
- **Update Entity**: Added update entity for tracking and managing software updates
- **MediaPlayer Entity**: Added media player entity for audio/video playback control (requires custom integration)

#### Integrations
- **Docker Integration**: Expose selected Docker containers as switches with attributes showing image name, status, and container ID
- **SystemD Integration**: Expose selected systemd user services as switches for start/stop control
- **MPRIS Integration**: Expose MPRIS media players over MQTT, automatically tracks currently active player (requires custom integration)
- **GameLauncher Integration**: Detect installed Steam/Heroic games and expose them as a dropdown menu in Home Assistant for voice-assisted game launching
- **Flatpak Updater Integration** (Flatpak only): Check for latest releases on GitHub and enable automatic installation via user-installed Flatpak setup
- **ActiveWindow Integration**: Updated to work in flatpak
- **Scripts Integration**: Added support for custom input variables, 
if exec line contains "{arg}", it exposes a textbox in HA and replaces {arg} with the input value

#### User Interface
- **KCM Improvements**: 
  - Dynamic tabs based on config file content
  - Support for creating/deleting scripts and shortcuts directly from UI
  - Auto-restart Kiot after pressing OK/Apply to ensure new configurations are used
- **System Tray Icon**: 
  - Green/yellow/red icon based on MQTT client connection state
  - Menu for opening settings, config file, and reconnecting

#### Helper Scripts
- **Main Helper Menu** (`helper.sh`): Interactive menu for easy installation and setup
- **Dependency Installer** (`scripts/dependencies.sh`): Automatic dependency installation for multiple distributions (apt/pacman)
- **Native Installer** (`scripts/native.sh`): Interactive menu for native build and installation
- **Flatpak Installer** (`scripts/flatpak.sh`): Interactive menu for Flatpak build and installation

#### Core Improvements
- **Version Management**: Updated CMakeLists to make project version available throughout codebase
- **Year Display**: Changed KAboutData from static "2024" to dynamic "2024-CurrentYear"
- **Config Validation**: Added config validator that opens KCM module and notifies user if configuration has issues
- **Auto-config Creation**: Basic config file creation and KCM module opening when required keys are missing

### Build System
- **Flatpak Support**: Added complete Flatpak build system with dedicated folder
- **CMake Options**: Added `BUILD_FLATPAK` option for conditional inclusion of Flatpak-only content
- **DBus Interfaces**: Automatic generation of DBus interface files during build
- **CI/CD**: Added auto-release pipeline for Flatpak builds

### Fixes and Improvements
- **Code Organization**: Removed monolithic entities.h and updated each file to include relevant headers
- **KCM Debugging**: Reduced excessive debug output from KCM module
- **Gitignore**: Updated to exclude build artifacts and temporary files
- **Flatpak Manifest**: Updated to provide necessary permissions for all integrations

### Documentation
- Updated README.md with new integrations and features
- Added comprehensive documentation for helper scripts
- Added this changelog file for tracking changes

## Notes
- This branch is highly experimental and intended for testing purposes
- Some features (like MediaPlayer and MPRIS) require custom Home Assistant integrations
- Helper scripts provide simplified installation for new users

---
