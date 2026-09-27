
# Kiot - SendSpin Plugin

## About

This plugin integrates a **SendSpin** client into KIOT, allowing you to stream audio to your KDE setup from Music Assistant.

The underlying client code is adapted from [Third Reality's Voice Music Assistant](https://github.com/thirdreality/voice-music-assistant/tree/linux-voice-assistant/buildroot/package/thirdreality/sendspin-client).

---

## Building & Installing

### 1. Native Build (Local Testing)
Make sure you have the required dependencies (Qt6, KF6, etc.) installed, then run:
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

```

### 2. Flatpak Extension Build

If you are running KIOT inside Flatpak and want to build this plugin as an extension module, use `flatpak-builder` with the provided YAML manifest:

```bash
# Clean up old build dirs if needed
rm -rf .flatpak-builder/ build-dir/

# Build and install locally for your user
flatpak-builder --user --install --force-clean build-dir org.davidedmundson.kiot.plugin.SendSpin.yml

```

---

## See Also

* [Experimental Plugins Overview](../README.md)
* [KIOT Main](../../README.md)



