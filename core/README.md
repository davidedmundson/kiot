# Kiot - Core

## Navigation

- [About](#about)
- [Core](#core)
- [Main](#main)
- [Appdata](#appdata)
- [Logging](#logging)
- [See Also](#see-also)


## About

Welcome to the Core folder! =)

This folder contains the core code used by the main binary to keep Kiot running smoothly.

The main purpose of this folder is to pave the way for an easier modular rewrite of the project in the future.

## Core

[core.cpp](core.cpp) is where the magic happens—this is where you can find the main loop and the core logic of the project. If you want to add a new core feature or understand how the runtime is orchestrated, this is the place to start.

## Main

[main.cpp](main.cpp) is the project entry point, handling initial startup arguments and bootstrapping the application.

## Appdata

The [appdata](appdata) folder contains metadata, icons, and desktop integration files for the project.

## Logging

The [logging](logging) folder houses a custom QtMessageHandler that provides organized, clean log output for easier debugging.

## See Also

- [KIOT Main](../README.md) for project overview and setup
- [KIOT Shared](../Shared/README.md)
- [KIOT Shared/Entities](/Shared/entities/README.md) for information about the shared lib entites part of the project
- [KIOT Integrations](../integrations/README.md) for creating new integrations
- [KIOT Example](../examples/README.md) for config examples and some inspiration
- [KIOT Helper Scripts](/scripts/README.md) for information about the helper scripts