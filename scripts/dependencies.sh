#!/bin/bash
# Dependency installer for Kiot
# Tested on Manjaro KDE, with Debian/Ubuntu package names researched
set -e

# Detect package manager
detect_pkg_manager() {
    if command -v apt >/dev/null 2>&1; then
        echo "apt"
    elif command -v pacman >/dev/null 2>&1; then
        echo "pacman"
    else
        echo "unknown"
    fi
}

# Install package
install_package() {
    local pkg="$1"
    local pm=$(detect_pkg_manager)
    if [ "$pm" = "apt" ]; then
        if ! dpkg -s "$pkg" >/dev/null 2>&1; then
            sudo apt update
            sudo apt install -y "$pkg"
        fi
    elif [ "$pm" = "pacman" ]; then
        if ! pacman -Qi "$pkg" >/dev/null 2>&1; then
            sudo pacman -S --noconfirm "$pkg"
        fi
    else
        echo "Unsupported package manager"
        exit 1
    fi
}

echo "Installing system dependencies for Kiot..."

PM=$(detect_pkg_manager)
if [ "$PM" = "unknown" ]; then
    echo "Cannot detect package manager. Aborting."
    exit 1
fi

echo "Detected package manager: $PM"

# Update system first
if [ "$PM" = "apt" ]; then
    sudo apt update
elif [ "$PM" = "pacman" ]; then
    sudo pacman -Sy
fi

# Core build tools (always needed)
core_packages=(
    cmake
    extra-cmake-modules
    git
)

# Qt6 dependencies
qt6_packages=()

# KDE Frameworks 6 dependencies
kf6_packages=()

# System libraries
system_packages=()

if [ "$PM" = "apt" ]; then
    # Debian/Ubuntu package names
    qt6_packages=(
        qt6-base-dev
        qt6-mqtt-dev
        qt6-declarative-dev  # For QML if needed
    )
    
    kf6_packages=(
        libkf6config-dev
        libkf6coreaddons-dev
        libkf6dbusaddons-dev
        libkf6notifications-dev
        libkf6idletime-dev
        libkf6globalaccel-dev
        libkf6kcmutils-dev
        libkf6i18n-dev
        libkf6solid-dev
        libkf6bluezqt-dev
        libkf6pulseaudioqt-dev
    )
    
    system_packages=(
        libudev-dev          # For gamepad integration (libudev)
        libsystemd-dev       # For systemd integration
    )
    
elif [ "$PM" = "pacman" ]; then
    # Arch/Manjaro package names
    qt6_packages=(
        qt6-base
        qt6-mqtt
        qt6-declarative      # For QML if needed
    )
    
    kf6_packages=(
        kconfig
        kcoreaddons
        kdbusaddons
        knotifications
        kidletime
        kglobalaccel
        kcmutils
        ki18n
        solid
        bluez-qt
        pulseaudio-qt
    )
    
    system_packages=(
        systemd-libs         # For udev and systemd
    )
fi

echo "Installing core build tools..."
for pkg in "${core_packages[@]}"; do
    install_package "$pkg"
done

echo "Installing Qt6 dependencies..."
for pkg in "${qt6_packages[@]}"; do
    install_package "$pkg"
done

echo "Installing KDE Frameworks 6 dependencies..."
for pkg in "${kf6_packages[@]}"; do
    install_package "$pkg"
done

echo "Installing system libraries..."
for pkg in "${system_packages[@]}"; do
    install_package "$pkg"
done

# Special handling for qt6-mqtt on Debian/Ubuntu
if [ "$PM" = "apt" ]; then
    # Check if qt6-mqtt-dev is available
    if ! apt-cache show qt6-mqtt-dev >/dev/null 2>&1; then
        echo "Warning: qt6-mqtt-dev not found in repositories."
        echo "You may need to install Qt6 MQTT module manually:"
        echo "  Option 1: Build from source (https://github.com/qt/qtmqtt)"
        echo "  Option 2: Use Qt Online Installer"
        echo "  Option 3: Check if your distribution has it in a different repository"
    fi
fi

echo ""
echo "=========================================="
echo "Dependency installation complete!"
echo "=========================================="

if [ "$PM" = "apt" ]; then
    echo "Note for Debian/Ubuntu users:"
    echo "  - qt6-mqtt-dev might not be available in all repositories"
    echo "  - If missing, you'll need to build Qt MQTT from source"
    echo "  - Visit: https://github.com/qt/qtmqtt for instructions"
elif [ "$PM" = "pacman" ]; then
    echo "All dependencies should now be installed."
    echo "You can proceed to build Kiot with:"

fi
