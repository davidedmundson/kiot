#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

NEW_PLUGIN_NAME=""
NEW_PLUGIN_CLASS=""
NEW_PLUGIN_DESCRIPTION=""
NEW_PLUGIN_REPOSITORY=""

pause() {
    echo
    read -rp "Press Enter to return to menu..."
}

get_plugin_variables()
{
    echo "Collecting information about your plugin to help you get a ready-to-go plugin"
    read -p "Plugin name (e.g. My Cool Plugin): " RAW_PLUGIN_NAME
    read -p "Short plugin description: " NEW_PLUGIN_DESCRIPTION
    read -p "Plugin Repository url: " NEW_PLUGIN_REPOSITORY

    # Fjern mellomrom og spesialtegn for klasse- og filnavn (PascalCase / ren streng)
    # F.eks. "Min Kul Plugin" blir "MinKulPlugin"
    NEW_PLUGIN_CLASS=$(echo "$RAW_PLUGIN_NAME" | tr -d '[:space:]-_')
    
    # Hvis du ønsker en merhet-vennlig mappe (f.eks. små bokstaver, fjernet mellomrom):
    NEW_PLUGIN_NAME=$(echo "$RAW_PLUGIN_NAME" | tr '[:upper:]' '[:lower:]' | tr '[:space:]' '_')

    echo "Using plugin class name: $NEW_PLUGIN_CLASS"
    echo "Using plugin directory/identifier: $NEW_PLUGIN_NAME"
    echo "Using plugin description: $NEW_PLUGIN_DESCRIPTION"
    echo "Using plugin repo: $NEW_PLUGIN_REPOSITORY"
}

copy_example()
{
    if [ -d "./examples/Plugins/Combined" ]; then
            echo "Copying example plugin..."
            cp -r ./examples/Plugins/Combined "./PluginsExperimental/$NEW_PLUGIN_NAME"
            echo "Your plugin is created in: ./PluginsExperimental/$NEW_PLUGIN_NAME"
    else
        echo "Example Plugins directory not found, sync your git repo to get the latest examples"
        echo "examples/Plugins/Combined"
        exit 1
    fi
}

replace_variables()
{
    cd "./PluginsExperimental/$NEW_PLUGIN_NAME/"
    echo "Starting to cleanup the template plugin"
    
    if [ -f "template.cpp" ]; then
        echo "Renaming template.cpp into $NEW_PLUGIN_CLASS.cpp "
        mv "template.cpp" "$NEW_PLUGIN_CLASS.cpp"
    else
        echo "File template.cpp does not exist."
    fi
    
    if [ -f "template.h" ]; then
        echo "Renaming template.h into $NEW_PLUGIN_CLASS.h "
        mv "template.h" "$NEW_PLUGIN_CLASS.h"
    else
        echo "File template.h does not exist."
    fi
    
    if [ -f "org.davidedmundson.kiot.Plugin.TemplatePlugin.yaml" ]; then
        echo "Renaming YAML into org.davidedmundson.kiot.Plugin.$NEW_PLUGIN_CLASS.yaml "
        mv "org.davidedmundson.kiot.Plugin.TemplatePlugin.yaml" "org.davidedmundson.kiot.Plugin.$NEW_PLUGIN_CLASS.yaml"
    else
        echo "File org.davidedmundson.kiot.Plugin.TemplatePlugin.yaml does not exist."
    fi
    
    if [ -f "org.davidedmundson.kiot.Plugin.TemplatePlugin.Metainfo.xml" ]; then
        echo "Renaming XML into org.davidedmundson.kiot.Plugin.$NEW_PLUGIN_CLASS.Metainfo.xml"
        mv "org.davidedmundson.kiot.Plugin.TemplatePlugin.Metainfo.xml" "org.davidedmundson.kiot.Plugin.$NEW_PLUGIN_CLASS.Metainfo.xml"
    else
        echo "File org.davidedmundson.kiot.Plugin.TemplatePlugin.Metainfo.xml does not exist."
    fi

    # Oppdater referanser i filer
    sed -i "s/template.h/$NEW_PLUGIN_CLASS.h/g" CMakeLists.txt
    sed -i "s/template.cpp/$NEW_PLUGIN_CLASS.cpp/g" CMakeLists.txt
    sed -i "s/template.h/$NEW_PLUGIN_CLASS.h/g" "$NEW_PLUGIN_CLASS.cpp"

    sed -i "s/TemplatePlugin/$NEW_PLUGIN_CLASS/g" CMakeLists.txt
    sed -i "s/TemplatePlugin/$NEW_PLUGIN_CLASS/g" "$NEW_PLUGIN_CLASS.h"
    sed -i "s/TemplatePlugin/$NEW_PLUGIN_CLASS/g" "$NEW_PLUGIN_CLASS.cpp"
    sed -i "s/TemplatePlugin/$NEW_PLUGIN_CLASS/g" plugin.json
    sed -i "s/TemplatePlugin/$NEW_PLUGIN_CLASS/g" "org.davidedmundson.kiot.Plugin.$NEW_PLUGIN_CLASS.yaml"
    sed -i "s/TemplatePlugin/$NEW_PLUGIN_CLASS/g" "org.davidedmundson.kiot.Plugin.$NEW_PLUGIN_CLASS.Metainfo.xml"

    sed -i "s/Template Plugin for KIOT/$NEW_PLUGIN_DESCRIPTION/g" CMakeLists.txt
    sed -i "s/Template Plugin for KIOT/$NEW_PLUGIN_DESCRIPTION/g" "$NEW_PLUGIN_CLASS.h"
    sed -i "s/Template Plugin for KIOT/$NEW_PLUGIN_DESCRIPTION/g" "$NEW_PLUGIN_CLASS.cpp"
    sed -i "s/Template Plugin for KIOT/$NEW_PLUGIN_DESCRIPTION/g" plugin.json
    sed -i "s/Template Plugin for KIOT/$NEW_PLUGIN_DESCRIPTION/g" "org.davidedmundson.kiot.Plugin.$NEW_PLUGIN_CLASS.yaml"
    sed -i "s/Template Plugin for KIOT/$NEW_PLUGIN_DESCRIPTION/g" "org.davidedmundson.kiot.Plugin.$NEW_PLUGIN_CLASS.Metainfo.xml"

    sed -i "s|https://github.com/davidedmundson/kiot|$NEW_PLUGIN_REPOSITORY|g" CMakeLists.txt
    sed -i "s|https://github.com/username/repo_name|$NEW_PLUGIN_REPOSITORY|g" "org.davidedmundson.kiot.Plugin.$NEW_PLUGIN_CLASS.Metainfo.xml"

    echo
    echo "======================================================================"
    echo " Your plugin is ready at: PluginsExperimental/$NEW_PLUGIN_NAME"
    echo "======================================================================"
    echo " NOTE: Since this plugin depends on KIOTShared, make sure you have"
    echo " installed the main KIOT project first (e.g., via helper.sh native menu"
    echo " or 'sudo cmake --install build'), otherwise CMake will fail!"
    echo "======================================================================"

    cd ../../
}

while true; do
    echo "======================================="
    echo " KIOT Plugin Creator"
    echo "======================================="
    echo
    echo "0 Quit "
    echo "1 Create new Plugin"
    echo
    echo "======================================="
    read -rp "Select an option: " choice

    case "$choice" in
        0)
            echo "Exiting."
            exit 0
            ;;
        1)
            get_plugin_variables
            copy_example
            replace_variables
            pause
            ;;
        *)
            echo "Invalid option. Please try again."
            pause
            ;;
    esac
done