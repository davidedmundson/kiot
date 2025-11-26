# Prøv å finne PulseAudioQt uten REQUIRED
find_package(KF6PulseAudioQt CONFIG)

include_directories(..)

set(KIOT_INTEGRATIONS_SRC
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
)

# Hvis PulseAudioQt ble funnet, ta med audio.cpp
if(KF6PulseAudioQt_FOUND)
    message(STATUS "PulseAudioQt found, enabling volume integration")
    list(APPEND KIOT_INTEGRATIONS_SRC audio.cpp)
else()
    message(WARNING "PulseAudioQt not found, skipping volume integration")
endif()

add_library(KIOTIntegrations OBJECT ${KIOT_INTEGRATIONS_SRC})

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

# Link PulseAudioQt kun hvis funnet
if(KF6PulseAudioQt_FOUND)
    target_link_libraries(KIOTIntegrations KF6::PulseAudioQt)
endif()
