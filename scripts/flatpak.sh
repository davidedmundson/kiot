#!/usr/bin/env bash
set -e
pause() {
    echo
    read -rp "Press Enter to return to menu..."
}

build() {
    mkdir -p build
    echo "Building flatpak bundle...."
    flatpak-builder --repo=flatpak-repo --force-clean build-installer .flatpak-manifest.yaml 
    echo "Building flatpak installer..........."
    flatpak build-bundle flatpak-repo ./build/kiot.flatpak org.davidedmundson.kiot testing_updated
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
    echo "1 Install as user (--user flag, no sudo needed)"
    echo "2 Uninstall as user (--user flag, no sudo needed)"
    echo "3 Install as system(--system flag, sudo needed)"
    echo "4 Uninstall as system(--system flag, sudo needed)"
    echo "5 Cleanup files"
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
            flatpak uninstall --user  -y org.davidedmundson.kiot
            pause
            ;;
        3)
            build
            echo "Installing as system..."
            sudo flatpak install --system  -y ./build/kiot.flatpak
            pause
            ;;
        4)
            echo "Uninstalling as system..."
            sudo flatpak uninstall --system  -y org.davidedmundson.kiot
            pause
            ;;
        5)
            echo "Deleting build and repo folders..."
            cleanup
            pause
            ;;
    esac
done