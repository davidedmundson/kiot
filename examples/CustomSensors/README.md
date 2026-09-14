# Kiot - CustonSensors Integration Examples and Generators

## Navigation

- [About](#about)
- [Nvidia](#nvidia)
  - [Power draw](#1-power-draw)
  - [Total Memory](#2-total-memory)
  - [Used Memory](#3-used-memory)
  - [Free Memory](#4-free-memory)
  - [Current Temperature](#5-current-temperature)
- [System](#system)
  - [CPU Temperature](#1-cpu-temperature)
  - [Uptime](#2-uptime)


## About

This is a collection of example configs for the CustomSensors Integration to help you get started
and hopefully find some inspiration for your own custom sensors.
More to come


## Nvidia

For nvidia everything should be universal and ready for use

### 1. **Power draw**
This is a simple example of how to get the power draw of your Nvidia GPU

<details>
<summary>Click to Expand Example</summary>

```toml
[CustomSensors][gpu_power]
name=Gpu power draw
command=nvidia-smi -q -d POWER | grep "Instantaneous Power Draw" -m 1 | cut -d':' -f2 | cut -d' ' -f2
device_class=power
interval=10s
state_class=measurement
unit_of_measurement=W
```

</details>

### 2. **Total Memory**
This is a simple example of how to get the temperature of your Nvidia GPU

<details>
<summary>Click to Expand Example</summary>

```toml
[CustomSensors][gpu_total_memory]
name=Gpu total memory
command=nvidia-smi -q -d MEMORY | grep "Total" -m 1 | cut -d':' -f2 | cut -d' ' -f2
interval=600s
state_class=measurement
unit_of_measurement=MiB
```
</details>

### 3. **Used Memory**
This is a simple example of how to get the used memory from your Nvidia GPU

<details>
<summary>Click to Expand Example</summary>

```toml
[CustomSensors][gpu_used_memory]
name=Gpu used memory
command=nvidia-smi -q -d MEMORY | grep "Used" -m 1 | cut -d':' -f2 | cut -d' ' -f2
interval=60s
state_class=measurement
unit_of_measurement=MiB
```
</details>

### 4. **Free Memory**
This is a simple example of how to get the free memory from your Nvidia GPU

<details>
<summary>Click to Expand Example</summary>

```toml
[CustomSensors][gpu_free_memory]
name=Gpu free memory
command=nvidia-smi -q -d MEMORY | grep "Free" -m 1 | cut -d':' -f2 | cut -d' ' -f2
interval=60s
state_class=measurement
unit_of_measurement=MiB
```
</details>

### 5. **Current temperature**
This is a simple example of how to get the current temperature from your Nvidia GPU

<details>
<summary>Click to Expand Example</summary>

```toml
[CustomSensors][gpu_current_temp]
name=Gpu temperature
command=nvidia-smi -q -d TEMPERATURE | grep "GPU Current Temp" -m 1 | cut -d':' -f2 | cut -d' ' -f2
interval=60s
state_class=measurement
unit_of_measurement=C
```
</details>


## System

Here we are using the package [lm-sensors](https://github.com/lm-sensors/lm-sensors) to get the temperatures from the system. You can install it with your package manager.

This will need customization to your system, but here are some examples of how to get the temperatures from your system.

We also use some other packages to get the information from the system, more info to come

### 1. **CPU temperature**
This is a simple example of how to get the cpu temperature from your system. this was made with a amd9950x3d
for intel something like this "sensors | grep 'Core 0' |  cut -d'+' -f 2 | cut -d'°' -f1"

<details>
<summary>Click to Expand Example</summary>

```toml
[CustomSensors][cpu_temperature]
name=CPU temperature
command=sensors | grep -m 1 'Tctl'  |  cut -d'+' -f 2 | cut -d'°' -f1
interval=60s
state_class=measurement
unit_of_measurement=C
```
</details>


### 2. **Uptime**
This is a simple example of how to get the uptime from your system in minutes.
<details>
<summary>Click to Expand Example</summary>

```toml
[CustomSensors][system_uptime]
name=System Uptime
command=awk '{print int($1/60)}' /proc/uptime
interval=60s
unit_of_measurement=min
```
</details>

### 3. **Free RAM**
This is a simple example of how to get the available free ram in GiB.
<details>
<summary>Click to Expand Example</summary>

```toml
[CustomSensors][free_ram]
name=Available RAM
command=free -h | awk '/^Mem:/ {print $7}' | cut -d'G' -f1
interval=300s
unit_of_measurement=GiB
```
</details>


### 4. **Total RAM**
This is a simple example of how to get the total ram in GiB.
<details>
<summary>Click to Expand Example</summary>

```toml
[CustomSensors][free_ram]
name=Available RAM
command=free -h | awk '/^Mem:/ {print $2}' | cut -d'G' -f1
interval=60000s
unit_of_measurement=GiB
```
</details>


### 5. **CPU usage**
This is a simple example of how to get the total cpu usage in %.
<details>
<summary>Click to Expand Example</summary>

```toml
[CustomSensors][cpu_usage]
name=CPU usage
command=top -bn1 | grep "%Cpu" | awk '{print 100 - $8}'
interval=300s
unit_of_measurement=%
```
</details>


More to come