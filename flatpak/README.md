# Flatpak Build for Kiot

This directory contains everything needed to build Kiot as a Flatpak and submit it to Flathub.

## Files

### Required for Building:
- `CMakeLists.txt` - CMake configuration for Flatpak builds
- `org.davidedmundson.kiot.desktop` - Desktop entry file
- `org.davidedmundson.kiot.metainfo.xml` - AppStream metadata
- `org.davidedmundson.kiot.svg` - Application icon (placeholder)
- `images/` - Screenshots for AppStream metadata

### For Flathub Submission:
- `org.davidedmundson.kiot.appdata.xml` - Extended AppStream metadata
- `flathub.json` - Flathub-specific configuration
- `compat-tab` - Compatibility information
- `validate-flathub.sh` - Validation script (executable)
- `FLATHUB_GUIDE.md` - Submission guide

### Helper Scripts:
- `generate-icons.sh` - Icon generation script (executable)
- `test-flatpak.sh` - Installation test script (executable)

## Building

### Using the Helper Script (Recommended)
```bash
./scripts/flatpak.sh
```

### Manual Build
```bash
# Build and install as user (no sudo required)
flatpak-builder --user --install --force-clean build .flatpak-manifest.yaml

# Or build and install system-wide (requires sudo)
flatpak-builder --install --force-clean build .flatpak-manifest.yaml
```

### Building a Bundle
```bash
# Build repository
flatpak-builder --repo=kiot-repo --force-clean build .flatpak-manifest.yaml

# Create bundle
flatpak build-bundle kiot-repo kiot.flatpak org.davidedmundson.kiot
```

## Testing

After installation, you can run Kiot with:
```bash
flatpak run org.davidedmundson.kiot
```

Run comprehensive tests:
```bash
cd flatpak
./test-flatpak.sh
```

## Preparing for Flathub

### 1. Validate Your Setup
```bash
cd flatpak
./validate-flathub.sh
```

### 2. Generate Proper Icons
```bash
cd flatpak
./generate-icons.sh
```

### 3. Read the Submission Guide
```bash
cat flatpak/FLATHUB_GUIDE.md
```

## Permissions

The Flatpak has been configured with permissions for:
- Network access (MQTT communication)
- Session and system D-Bus
- Wayland and X11 display servers
- PulseAudio for audio control
- Home directory access for configuration
- Docker socket access (for Docker integration)
- SystemD access (for service management)

## Notes

### Icon
The current icon (`org.davidedmundson.kiot.svg`) is a placeholder. Before publishing to Flathub, you should:
1. Create a proper icon that represents Kiot/IoT
2. Generate multiple sizes using `./generate-icons.sh`

### Screenshots
The screenshots in `images/` directory are referenced in the metainfo.xml file. Before publishing:
1. Update screenshot URLs in metadata files to point to the final repository
2. Ensure all referenced screenshots exist in the `images/` directory
3. Screenshots should be at least 800x600 pixels

### Sandbox Limitations
Some features may have limited functionality in the Flatpak sandbox:
- Hardware access may be restricted
- Some system integrations may require additional permissions
- File system access is limited to approved locations


## Troubleshooting

### Common Issues

1. **Missing permissions**: If an integration doesn't work, check if it needs additional Flatpak permissions
2. **File system access**: Configuration files must be in accessible locations

### Debugging
```bash
# Run with debug output
flatpak run --command=sh org.davidedmundson.kiot

# Check logs
journalctl --user -f | grep kiot


```

## Development

To modify the Flatpak configuration:
1. Edit `.flatpak-manifest.yaml` in the root directory
2. Update permissions in the `finish-args` section as needed
3. Test changes with `flatpak-builder build .flatpak-manifest.yaml --user --install --force-clean`
