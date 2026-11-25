
# Design

Kiot is built around the design of "plugins" living in the integration folder. These represent a semantic task, lockscreen, nightmode, etc.

Each integration in turn creates multiple entities. For example nightmode might expose the current brightness as a sensor and a switch for whether we're inhibited. Entities represent the entities you'd see in Home Assistant. Each entity can have IDs and names and icons. See the MQTT docs in Home Assistant. 

Entity subclasses exist for BinarySensor, Sensor and so on, exposing convenience API.

The job of each Entity subclass is to abstract MQTT away from the logic code handling reconnects transparently from the logic code.

# Creating Integrations

There's lots of things that can be exposed, we can go wild if you have new automation ideas! Don't expose just because you can, having your font size in HA would be a bit silly. 

Drop a file into the integrations folder and add it to the CMakeLists.txt. There is a macro REGISTER_INTEGRATION which takes a function to run on startup to set everything up. You can either perform all the logic in here, or use a wrapper class depending on the scope of the task.

# Why MQTT?

It seems like it would be sane to use native integration (like the mobile phone), but it didn't pan out.

The native integration is fine for sensors, but you can't expose switches or actions very well, you can only do string matching on notification text which felt hacky all round. 

# Tests

The tests are in `autotests/` and build with the rest (`BUILD_TESTING`, on by default). Run them from the build directory:

```sh
ctest --output-on-failure
```

`mqttentitiestest` starts a mosquitto broker of its own (the `mosquitto` package) and skips without one; `KIOT_MQTT_TESTS=required` turns that skip into a failure. No test touches your own `~/.config/kiotrc` or desktop session: each runs with temporary XDG directories and no session bus.

A test that pins a known bug uses `QEXPECT_FAIL`: it passes while the bug is there and fails once the bug is fixed, so remove the `QEXPECT_FAIL` together with the fix.

The workflow `.github/workflows/tests.yml` builds everything and runs the tests on every push, on GitHub and on Forgejo alike; the Flatpak workflows run on GitHub only.
