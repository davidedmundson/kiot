# Kiot - Helper Scripts

## Navigation

- [About](#about)
- [Recommended Usage](#recommended-usage)
- [Available Scripts](#available-scripts)
  - [dependencies.sh](#dependenciessh)
  - [native.sh](#nativesh)
  - [flatpak.sh](#flatpaksh)
- [Usage](#usage)
- [Notes](#notes)
- [See Also](#see-also)

## About

This directory contains helper scripts to simplify Kiot installation and development.

## Recommended Usage

The easiest way to use these scripts is via the top-level **`helper.sh`** menu in the repository root:
```bash
chmod +x helper.sh
./helper.sh
```
## Available Scripts

### `dependencies.sh`
Installs all required dependencies for your Linux distribution.

- Detects package manager (apt/pacman)
- Installs Qt6, KDE Frameworks 6, and system libraries


### `native.sh`
Interactive menu for native installation (using `sudo make install`).

- Build Kiot from source
- Install system-wide
- Uninstall previously installed versions
- Clean build directory

### `flatpak.sh`  
Interactive menu for Flatpak installation.

- Build Flatpak bundle
- Install as user or system
- Uninstall Flatpak version
- Clean build artifacts

## Usage

1. Make scripts executable:
   ```bash
   chmod +x scripts/*.sh
   ```

2. Install dependencies:
   ```bash
   ./scripts/dependencies.sh
   ```

3. Choose installation method:
   ```bash
   # For native installation
   ./scripts/native.sh
   
   # For Flatpak installation
   ./scripts/flatpak.sh
   ```

## Notes
- The `native.sh` script requires `sudo` for installation
- The `flatpak.sh` script can install without `sudo` using `--user` flag
- Dependencies may vary between distributions


## See Also

- [KIOT Main](../README.md) for project overview and setup
- [KIOT Core](../core/README.md) for information about the core bin part of the project
- [KIOT Shared](../Shared/README.md)
- [KIOT Shared/Entities](/Shared/entities/README.md) for information about the shared lib entites part of the project
- [KIOT Integrations](../integrations/README.md) for creating new integrations
- [KIOT Example](../examples/README.md) for config examples and some inspiration
