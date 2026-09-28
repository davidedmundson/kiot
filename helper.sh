#!/usr/bin/env bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_PATH="$SCRIPT_DIR/scripts"

# Defensive sjekk: Finnes scripts-mappen i det hele tatt?
if [ ! -d "$SCRIPTS_PATH" ]; then
    echo "Error: Directory '$SCRIPTS_PATH' not found."
    echo "Make sure you are running helper.sh from the root of the KIOT repository."
    exit 1
fi

pause() {
    echo
    read -rp "Press Enter to return to menu..."
}

# Hjelpefunksjon som sjekker om skriptet eksisterer før kjøring
run_script() {
    local script_name="$1"
    local full_path="$SCRIPTS_PATH/$script_name"

    if [ -f "$full_path" ]; then
        bash "$full_path"
    else
        echo "Error: Could not find script '$script_name' in $SCRIPTS_PATH"
    fi
}

while true; do
    clear
    echo "======================================="
    echo " KIOT Helper Menu"
    echo "======================================="
    echo
    echo "0 Quit"
    echo "1 Install dependencies (Arch/Debian, tested on Manjaro via pacman)"
    echo "2 Native build and install menu"
    echo "3 Flatpak build and install menu"
    echo "4 Genereate new plugin from template"
    echo
    echo "======================================="
    read -rp "Select an option: " choice

    case "$choice" in
        0)
            echo "Exiting."
            exit 0
            ;;
        1)
            echo "Installing dependencies..."
            run_script "dependencies.sh"
            pause
            ;;
        2)
            echo "Opening Native build menu..."
            run_script "native.sh"
            pause
            ;;
        3)
            echo "Opening Flatpak build menu..."
            run_script "flatpak.sh"
            pause
            ;;
        4)
            echo "Generating new plugin..."
            run_script "plugincreator.sh"
            pause
            ;;
        *)
            echo "Invalid option."
            sleep 1
            ;;
    esac
done
