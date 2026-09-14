#!/bin/bash

#Checks if kscreen-doctor is installed
if ! command -v "kscreen-doctor" &>/dev/null; then
    echo "kscreen-doctor does not exist, aborting"
    exit 1
fi
#Checks if hwinfo is installed
if ! command -v "hwinfo" &>/dev/null; then
    echo "hwinfo does not exist, aborting"
    exit 1
fi
#Sets our output file
OUTPUT_FILE="displays.conf"
rm -f "$OUTPUT_FILE"

echo "Collecting display information..."
#Tries to map the ports from kscreen-doctor
mapfile -t ports < <(kscreen-doctor --outputs | grep "Output:" | awk '{print $3}')
echo "Found ports: ${#ports[@]}"
#Tries to map the models from hwinfo, if it fails, it will use a standard display name
mapfile -t models < <(hwinfo --monitor 2>/dev/null | grep -E 'Model:' | cut -d':' -f2)
echo "Found screen models: ${#models[@]}"

if [ ${#models[@]} -eq 0 ]; then
    models=("Standard Display 1" "Standard Display 2" "Standard Display 3")
fi

echo "-----------------------------------"
echo "Found screens:"
#Iterates over the ports and models to generate your config file
for index in "${!ports[@]}"; do
    port="${ports[$index]}"
    model_name="${models[$index]:-$port}"
    cleaned="${model_name//\"/}"

    echo "Chose$cleaned for port $port"

    id_name=$(echo "$cleaned" | tr '[:upper:]' '[:lower:]' | tr -cd '[:alnum:]_')

    cat << EOF >> "$OUTPUT_FILE"
[Scripts][display_${id_name}_primary]
Name=Set$cleaned as Primary
Exec=kscreen-doctor output.$port.primary
Icon=mdi:monitor

[Scripts][display_${id_name}_enable]
Name=Set$cleaned as Enabled
Exec=kscreen-doctor output.$port.enable
Icon=mdi:monitor

[Scripts][display_${id_name}_disable]
Name=Set$cleaned as Disabled
Exec=kscreen-doctor output.$port.disable
Icon=mdi:monitor

[Scripts][display_${id_name}_rotate_left]
Name=Rotate$cleaned to the left
Exec=kscreen-doctor output.$port.rotation.left
Icon=mdi:monitor

[Scripts][display_${id_name}_rotate_right]
Name=Rotate$cleaned to the right
Exec=kscreen-doctor output.$port.rotation.right
Icon=mdi:monitor

[Scripts][display_${id_name}_hdr_enable]
Name=HDR$cleaned Enable
Exec=kscreen-doctor output.$port.hdr.enable
Icon=mdi:monitor

[Scripts][display_${id_name}_hdr_disable]
Name=HDR$cleaned Disable
Exec=kscreen-doctor output.$port.hdr.disable
Icon=mdi:monitor

EOF
done

echo "Saved this config as ./$OUTPUT_FILE"
echo "Save the scripts you want in kiot config at: $HOME/.config/kiotrc"
echo "And restart kiot to load them to home assistant"
cat "$OUTPUT_FILE"
