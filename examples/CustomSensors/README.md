# Kiot - CustonSensors Integration Examples and Generators

## Navigation

- [About](#about)
- [Nvidia](#nvidia)
  - [Power draw](#1-power-draw)
  - [Total Memory](#2-total-memory)
  - [Used Memory](#3-used-memory)
  - [Free Memory](#4-free-memory)
  - [Current Temperature](#5-current-temperature)


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