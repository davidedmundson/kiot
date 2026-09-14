#!/bin/bash

#Makes sure we are in the correct directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/"
# Check that kiot is installed first
if ! command -v "kiot" &>/dev/null; then
    echo "kiot is not installed, aborting until its found"
    exit 1
fi
#Path to our service file
SERVICE_FILE="kiot.service"
#Where we want to install it
TARGET_DIR="$HOME/.config/systemd/user"

#Check for our service file
if [ ! -f "$SERVICE_FILE" ]; then
    echo "Error: $SERVICE_FILE not found in the current directory."
    exit 1
fi

echo "Setting up Kiot systemd user service..."

# Creates our install folder if it does not already exist
mkdir -p "$TARGET_DIR"

# Copy the service file to the target directory
cp "$SERVICE_FILE" "$TARGET_DIR/"
echo "Copied $SERVICE_FILE to $TARGET_DIR/"

# Reload, enable and start
systemctl --user daemon-reload
systemctl --user enable --now kiot.service

echo "-----------------------------------"
echo "Kiot user service is installed and started!"
echo "Check status with: systemctl --user status kiot.service"