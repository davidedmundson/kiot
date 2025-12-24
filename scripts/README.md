# Helper Scripts for Kiot

This directory contains helper scripts to simplify Kiot installation and development.

## Available Scripts

### `dependencies.sh`
Installs all required dependencies for your Linux distribution.
- Detects package manager (apt/pacman)
- Installs Qt6, KDE Frameworks 6, and system libraries
- Handles distribution-specific package names

### `native.sh`
Interactive menu for native installation (using `make install`).
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