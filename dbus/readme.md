# Kiot - Shared

## Navigation

- [About](#about)
- [How to create](#how-to-create)
- [How to add to cmakelists](#how-to-add-to-cmakelists)
- [See Also](#see-also)


## About

Welcome to the dbus folder! =)

This folder contains xml files that describe the dbus interfaces of the applications. These files are used to generate the dbus interfaces for the applications.

 
## How to create

So just as a reminder for myself in the future for how the xml files was generated...

```sh
qdbus  org.kde.KWin /org/kde/KWin/NightLight  org.freedesktop.DBus.Introspectable.Introspect > org.kde.KWin.Night.xml
```

## How to add to cmakelists

And a example of cmakelists implementation to make the project genereate the interface on buildtime,
if the cmakelists already got this somewhere, just expand on the already implemented part

```
set_source_files_properties(
    org.kde.KWin.NightLight.xml
    PROPERTIES
    NO_NAMESPACE ON
)
qt_add_dbus_interface(TARGET_SOURCES org.kde.KWin.NightLight.xml kwinnightlight)
```

## See Also

- [KIOT Core](/core/README.md) for information about the core bin part of the project
- [KIOT Shared](/Shared/README.md) for information about the shared lib part of the project
- [KIOT Shared/Entities](/Shared/entities/README.md) for information about the shared lib entites part of the project
- [KIOT Integrations](/integrations/README.md) for creating new integrations
- [KIOT Examples](/examples/README.md) for config examples and some inspiration
- [KIOT Helper Scripts](/scripts/README.md) for information about the helper scripts

---
