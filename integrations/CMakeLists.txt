
# Prøv å finne libudev via pkg-config, men uten å kreve at den finnes
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUDEV libudev)

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
    audio.cpp
    battery.cpp
)

# Include gamepad.cpp only when libudev library is found
if(LIBUDEV_FOUND)
    message(STATUS "libudev found, enabling gamepad integration")
    list(APPEND KIOT_INTEGRATIONS_SRC gamepad.cpp)
    include_directories(${LIBUDEV_INCLUDE_DIRS})
else()
    message(WARNING "libudev not found, skipping gamepad integration")
endif()

add_library(KIOTIntegrations OBJECT ${KIOT_INTEGRATIONS_SRC})

# Always link main dependencies
target_link_libraries(KIOTIntegrations
    KF6::IdleTime
    KF6::GlobalAccel
    KF6::Notifications
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::PulseAudioQt
    KF6::Solid
    Qt6::Gui
    Qt6::Widgets
    Qt6::Mqtt
    Qt6::DBus

)


# Link libudev if found
if(LIBUDEV_FOUND)
    target_link_libraries(KIOTIntegrations ${LIBUDEV_LIBRARIES})
    link_directories(${LIBUDEV_LIBRARY_DIRS})
    add_definitions(${LIBUDEV_CFLAGS_OTHER})
endif()

# Install KWin script used by ActiveWindowWatcher
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/activewindow_kwin.js
    DESTINATION ${KDE_INSTALL_DATADIR}/kiot
)
