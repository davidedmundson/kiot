#!/usr/bin/env bash
set -e

pause() {
    echo
    read -rp "Press Enter to return to menu..."
}

cleanup() {
    echo "Cleaning up build directory..."
    rm -rf build
}

build() {
    echo "Building kiot..."
    mkdir -p build
    cd build
    cmake ..
    make clean
   # make clang-format
    make
    cd ..
}

install() {
    echo "Installing kiot...(this requires sudo)"
    cd build
    sudo make install
    cd ..
}

build_and_install() {
    echo "Building and installing kiot..."
    build
    install
}

uninstall() {
    echo "Uninstalling kiot...(this requires sudo)"
    
    # Check if install manifest exists
    if [ -f "build/install_manifest.txt" ]; then
        echo "Using install manifest to uninstall..."
        # Read each line from manifest and remove the file
        while IFS= read -r file; do
            if [ -e "$file" ]; then
                echo "Removing: $file"
                sudo rm -f "$file"
                
                # Remove empty parent directories
                dir=$(dirname "$file")
                while [ "$dir" != "/" ]; do
                    if rmdir "$dir" 2>/dev/null; then
                        echo "Removed empty directory: $dir"
                    else
                        break
                    fi
                    dir=$(dirname "$dir")
                done
            fi
        done < "build/install_manifest.txt"
        
        # Also remove common known files as backup
        echo "Removing known installed files..."
        sudo rm -f /usr/bin/kiot
        sudo rm -f /etc/xdg/autostart/org.davidedmundson.kiot.desktop
        sudo rm -f /usr/share/applications/org.davidedmundson.kiot.desktop
        sudo rm -f /usr/share/kiot/activewindow_kwin.js
        sudo rm -f /usr/lib/qt6/plugins/plasma/kcms/systemsettings/kcm_kiot.so
        sudo rm -f /usr/share/applications/kcm_kiot.desktop
        
        # Remove empty directories
        sudo rmdir /usr/share/kiot 2>/dev/null || true
        
        echo "Uninstall completed!"
    else
        echo "Install manifest not found. Removing known files..."
        # Manual removal if manifest doesn't exist
        sudo rm -f /usr/bin/kiot
        sudo rm -f /etc/xdg/autostart/org.davidedmundson.kiot.desktop
        sudo rm -f /usr/share/applications/org.davidedmundson.kiot.desktop
        sudo rm -f /usr/share/kiot/activewindow_kwin.js
        sudo rm -f /usr/lib/qt6/plugins/plasma/kcms/systemsettings/kcm_kiot.so
        sudo rm -f /usr/share/applications/kcm_kiot.desktop
        
        # Remove empty directories
        sudo rmdir /usr/share/kiot 2>/dev/null || true
        
        echo "Uninstall completed (manual mode)!"
    fi
    
    # Update desktop database
    echo "Updating desktop database..."
    sudo update-desktop-database 2>/dev/null || true
}

while true; do
    echo "======================================="
    echo " KIOT Native Installer Menu"
    echo "======================================="
    echo
    echo "0 Quit (runs cleanup before closing)"
    echo "1 Build kiot"
    echo "2 Install kiot (use 1 first, this requires sudo)"
    echo "3 Build and install kiot (AIO, this requires sudo)"
    echo "4 Uninstall (requires sudo, this will remove all file)"
    echo "5 Cleanup (Deletes the build folder)"
    echo
    echo "======================================="
    read -rp "Select an option: " choice

    case "$choice" in
        0)
            echo "Exiting."
            exit 0
            ;;
        1)
            build
            pause
            ;;
        2)
            echo "Installing kiot"
            install
            pause
            ;;
        3)
            echo "Building and installing kiot..."
            build_and_install
            pause
            ;;
        4)
            echo "Uninstalling kiot"
            uninstall
            pause
            ;;
        4)
            echo "Cleaning up after us"
            cleanup
            pause
            ;;
        *)
            echo "Invalid option. Please try again."
            pause
            ;;
    esac
done