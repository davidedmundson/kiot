# Kiot - System examples

## Navigation

- [About](#about)
- [Autostart](#autostart)
  - [Installation](#installation)
  - [Uninstallation](#uninstallation)

## About

This is a collection of examples on system setup to help you get started.

## Autostart

Example of a systemd user service to autostart Kiot on login. For a copy-paste ready version, check the [kiot.service](/examples/System/kiot.service) file.
<details>
<summary>Click to Expand Example Output</summary>

```toml
[Unit]
# This is just a simple description of our tool
Description=A linux mqtt bridge for use with home assistant
# A link back to the repo for more info
Documentation=https://github.com/davidedmundson/kiot
# Tells systemd kiot wants networking
Wants=network-online.target
# Tells systemd kiot should start after networking and graphical session are up
After=network-online.target graphical-session.target

[Service]
Type=simple
# Tells systemd where our binary is found
ExecStart=/usr/bin/kiot
# Tells systemd to restart kiot if it fails
Restart=on-failure
# Tells system to wait 3 sec before a restart
RestartSec=3

Slice=user.slice

[Install]
WantedBy=graphical-session.target
```


</details>

### Installation

To automatically set up and start Kiot as a systemd user service, run the provided helper script:

```bash
./install_systemd.sh
```

### Uninstallation
**Note: This will remove the service but not kiot.**
To cleanly stop, disable, and remove the systemd user service from your system, run the uninstallation script:

```bash
./uninstall_systemd.sh
```