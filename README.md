# About

Kiot (KDE Internet Of Things) is a background daemon that exposes useful information and actions for your local desktop session to a home automation controller like Home Assistant.

This does not control smart home devices directly. i.e:
If you want a light to turn on when the PC is set to "Do not Disturb" mode, this app will not directly control the light. This app exposes the "Do not distrub" state to your controller (Home Assistant) so that you can create an automation there.

# Current State

This is pre-alpha software a level where if you're ok compiling things from source and meddling with config files by hand.

# Setup

## Dependencies

Make sure you have these packages installed
- `cmake`
- `extra-cmake-modules`
- `qt6-base`/`qt6-base-dev`
- `qt6-mqtt`/`qt6-mqtt-dev`

(beware that depending on your distribution, these package names may vary slightly)

## Download and install

Download this repo, for example, by cloning it: 
```sh
git clone https://github.com/davidedmundson/kiot.git  # downloads the repo to your system
cd kiot  # switches directory to the newly downloaded folder
```
Now, launch the following commands to proceed with installation:
```sh
mkdir build
cd build
cmake ..
make
make install  # might require `sudo`
```
Some dependencies might be missing, make sure you have 

# MQTT

In home assistant MQTT server must be enabled.
See https://www.home-assistant.io/integrations/mqtt/

The following configuration needs to be placed in `~/.config/kiotrc`,

```
 [general]
 host=some.host
 port=1883
 user=myUsername
 password=myPassword
 ```

- `host` should be your Home Assistant local address,
- `port` is correct at 1883 by default,
- `user` and `password` should be the username and password of a Home Assistant user (**recommended to create a specific user for MQTT connection**)

On the home assistant side everything should then work out-the-box with MQTT discovery.
Try rebooting Home Assistant, and then launch the `kiot` program and see it things go well. 

# Goals

Compared to other similar projects, I want to avoid exposing pointless system statistic information that's not useful in a HA context. There's no point having a sensor for "kernel version" for example. Instead the focus is towards tighter desktop integration with things that are practical and useful. This includes, but is not exclusive too some Plasma specific properties.

The other focus is on ensuring that device triggers and actions appear in an intuitive easy-to-use way in Home Assistant's configuration. 

# Supported Features (so far)

 - User activity (binary sensor)
 - Locked state (switch)
 - Suspend (button)
 - Camera in use (binary sensor)
 - Accent Colour (sensor)
 - Arbitrary Scripts (buttons)
 - Shortcuts (device_trigger)
 - Nightmode status (binary sensor)

 
# Additional Config

```
[general]
host=some.host
port=1883
user=myUsername
password=myPassword
useSSL=false

[Scripts][myScript1]
Name=Launch chrome
Exec=google-chrome

[Scripts][myScript2]
...

[Shortcuts][myShortcut1]
Name=Do a thing
# This then becomes available in global shortcuts KCM for assignment and will appear as a trigger in HA, so keys can be bound to HA actions


```

 
