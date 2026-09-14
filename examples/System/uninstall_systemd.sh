#!/bin/bash

SERVICE_FILE="kiot.service"
TARGET_DIR="$HOME/.config/systemd/user"
TARGET_PATH="$TARGET_DIR/$SERVICE_FILE"

echo "Stopping and disabling Kiot user service..."
systemctl --user disable --now kiot.service 2>/dev/null

if [ -f "$TARGET_PATH" ]; then
    rm -f "$TARGET_PATH"
    echo "Removed $TARGET_PATH"
else
    echo "Notice: Service file not found at $TARGET_PATH"
fi

# Reload systemd user daemon to clear it out
systemctl --user daemon-reload

echo "-----------------------------------"
echo "Kiot user service has been successfully uninstalled."
echo "-----------------------------------"