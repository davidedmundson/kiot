#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

#Grabs the flatpak id dynamic from the manifest with fallback to david
FLATPAK_ID=$(grep -E '^id:' .org.davidedmundson.kiot.yaml | awk '{print $2}' | tr -d '"' | tr -d "'")

# Hvis den feiler, sett en trygg fallback
if [ -z "$FLATPAK_ID" ]; then
    FLATPAK_ID="org.davidedmundson.kiot"
fi


pause() {
    echo
    read -rp "Press Enter to return to menu..."
}

build() {
    if ! command -v flatpak-builder &> /dev/null; then
        echo "Error: flatpak-builder is not installed. Please install it first."
        return 0
    fi
    mkdir -p build
    echo "Building flatpak bundle...."
    flatpak-builder --repo=flatpak-repo --force-clean build-installer .org.davidedmundson.kiot.yaml 
    echo "Building flatpak installer..........."
    flatpak build-bundle flatpak-repo ./build/kiot.flatpak $FLATPAK_ID master
    echo "installer buildt and located at ./build/kiot.flatpak"
}

cleanup() {
    echo "Cleaning up files..."
    rm -rf ./build-installer/
    rm -rf ./flatpak-repo/
}
while true; do
    echo "======================================="
    echo " KIOT Flatpak Installer Menu"
    echo "======================================="
    echo
    echo "0 Quit (runs a cleanup before closing)"
    echo "1 Build and Install as user (--user flag, no sudo needed)"
    echo "2 Uninstall as user (--user flag, no sudo needed)"
    echo "3 Cleanup files"
    echo
    echo "======================================="
    read -rp "Select an option: " choice

    case "$choice" in
        0)
            echo "Exiting."
            cleanup
            exit 0
            ;;
        1)
            build
            echo "Installing kiot as user..."
            flatpak install --user  -y ./build/kiot.flatpak
            pause
            ;;
        2)
            echo "Uninstalling kiot as user..."
            flatpak uninstall --user  -y $FLATPAK_ID
            pause
            ;;
        3)
            echo "Deleting build and repo folders..."
            cleanup
            pause
            ;;
    esac
done