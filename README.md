# About

Kiot (KDE Internet Of Things) is a background daemon that exposes useful information and actions for your local desktop session to a home automation controller like Home Assistant.

This does not control smart home devices directly. i.e:
If you want a light to turn on when the PC is set to "Do not Disturb" mode, this app will not directly control the light. This app exposes the "Do not distrub" state to your controller (Home Assistant) so that you can create an automation there.

# Current State

This is pre-alpha software a level where if you're ok compiling things from source and meddling with config files by hand.

# Setup

mkdir build
cd build
cmake ..
make
make install

# MQTT

In home assistant MQTT server must be enabled.
See https://www.home-assistant.io/integrations/mqtt/

The following configuration needs to be placed in .config/kiotrc

```
 [general]
 host=some.host
 port=1883
 user=myUsername
 password=myPassword
 ```

On the home assistant side everything should then work out-the-box with MQTT discovery.

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

 
