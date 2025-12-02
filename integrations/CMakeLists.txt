# Prøv å finne PulseAudioQt uten REQUIRED
find_package(KF6PulseAudioQt CONFIG)

# Prøv å finne SDL2 uten REQUIRED
find_package(SDL2 CONFIG)
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

# Include audio.cpp only when PulseAudioQt library is found
if(KF6PulseAudioQt_FOUND)
    message(STATUS "PulseAudioQt found, enabling volume integration")
    list(APPEND KIOT_INTEGRATIONS_SRC audio.cpp)
else()
    message(WARNING "PulseAudioQt not found, skipping volume integration")
endif()

# Include gamepad.cpp only when sdl2 library is found
if(SDL2_FOUND)
    message(STATUS "SDL2 found, enabling gamepad integration")
    list(APPEND KIOT_INTEGRATIONS_SRC gamepad.cpp)
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

# Link PulseAudioQt if library is found
if(KF6PulseAudioQt_FOUND)
    target_link_libraries(KIOTIntegrations KF6::PulseAudioQt)
endif()
# Link SDL2 only if library is found
if(SDL2_FOUND)
    target_link_libraries(KIOTIntegrations SDL2::SDL2)
endif()
# Install KWin script used by ActiveWindowWatcher
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/activewindow_kwin.js
    DESTINATION ${KDE_INSTALL_DATADIR}/kiot
)
