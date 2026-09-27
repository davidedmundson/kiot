# Kiot - Experimental Plugins

## Navigation

- [About](#about)
- [SendSpin](#sendspin)
- [See Also](#see-also)

## About

Welcome to the experimental plugins folder! =)

This is where we test out new plugins that don't quite fit into KIOT's core MQTT protocol, but are used to expand what KIOT can do. Hopefully, this folder can also serve as a source of inspiration if you want to build your own plugins in the future.

Nothing in this folder is included in the default build. These plugins need to be compiled manually as a native library or a Flatpak extension depending on your setup.

Third-party and alternative plugins like these can typically be installed in:
- **Native setup:** `~/.local/share/kiot/plugins/`
- **Flatpak (manual plugin):** `~/.var/app/org.davidedmundson.kiot/data/kiot/plugins/`
- Or you can build a dedicated Flatpak extension for maximum compatibility!

---

## SendSpin

SendSpin is a port of the client code found in Third Reality's repository: [thirdreality/voice-music-assistant](https://github.com/thirdreality/voice-music-assistant/tree/linux-voice-assistant/buildroot/package/thirdreality/sendspin-client). 

*Note: All credits for the underlying code go to them; I only adapted it to work as a modular KIOT plugin.*

- **Directory:** [`SendSpin`](./SendSpin/README.md)

---

## See Also

- [KIOT Main](../README.md) for project overview and setup
- [KIOT Shared](../Shared/README.md)
- [KIOT Shared/Entities](../Shared/entities/README.md) for info about shared lib entities
- [KIOT Integrations](../integrations/README.md) for creating new integrations
- [KIOT Examples](../examples/README.md) for config examples and inspiration
- [KIOT Helper Scripts](../scripts/README.md) for info about helper scripts