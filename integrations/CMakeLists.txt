include_directories(..)
add_library(KIOTIntegrations OBJECT
    active.cpp
    notifications.cpp
    scripts.cpp
    lockedstate.cpp
    shortcuts.cpp
    suspend.cpp
    login1_manager_interface.cpp
    dndstate.cpp
    nightmode.cpp
    camera.cpp
    accentcolour.cpp
    activewindow.cpp
    volume.cpp
)

target_link_libraries(KIOTIntegrations
    KF6::IdleTime
    KF6::GlobalAccel
    KF6::Notifications
    KF6::ConfigCore
    KF6::CoreAddons
    Qt6::Gui
    Qt6::Widgets
    Qt6::Mqtt
    Qt6::DBus
)
