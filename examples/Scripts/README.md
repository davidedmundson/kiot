# Kiot - Scripts Integration Examples and Generators

## Navigation
- [About](#about)
- [Display](#display)

## About

This is a collection of example configs for different scripts that can be used with Kiot, along with helper scripts to generate them automatically on your own computer.

## Display
Examples of scripts to control your display configuration. You can use the pre generated and modify for you needs [displays.conf](/examples/Scripts/displays.conf) or run the generator script [display_generator.sh](/examples/Scripts/display_generator.sh) locally to have it create correct scripts for your computer.

<details>
<summary>Click to Expand Example Output</summary>

```toml
[Scripts][display_philipsphilipsftv_primary]
Name=Set PHILIPS Philips FTV as Primary
Exec=kscreen-doctor output.DP-3.primary
Icon=mdi:monitor

[Scripts][display_philipsphilipsftv_enable]
Name=Set PHILIPS Philips FTV as Enabled
Exec=kscreen-doctor output.DP-3.enable
Icon=mdi:monitor

[Scripts][display_philipsphilipsftv_disable]
Name=Set PHILIPS Philips FTV as Disabled
Exec=kscreen-doctor output.DP-3.disable
Icon=mdi:monitor

[Scripts][display_philipsphilipsftv_rotate_left]
Name=Rotate PHILIPS Philips FTV to the left
Exec=kscreen-doctor output.DP-3.rotation.left
Icon=mdi:monitor

[Scripts][display_philipsphilipsftv_rotate_right]
Name=Rotate PHILIPS Philips FTV to the right
Exec=kscreen-doctor output.DP-3.rotation.right
Icon=mdi:monitor

[Scripts][display_philipsphilipsftv_hdr_enable]
Name=HDR PHILIPS Philips FTV Enable
Exec=kscreen-doctor output.DP-3.hdr.enable
Icon=mdi:monitor

[Scripts][display_philipsphilipsftv_hdr_disable]
Name=HDR PHILIPS Philips FTV Disable
Exec=kscreen-doctor output.DP-3.hdr.disable
Icon=mdi:monitor
```
</details>


## More to come, requests are welcome