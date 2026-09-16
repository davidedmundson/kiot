# UI components for kiot (replaces KCM)
include_directories(../..)

# Source files for UI
set(KIOT_UIQT_SOURCES
    mainwindow.cpp
    mainwindow.h
    settingsmanager.cpp
    settingsmanager.h
)

# Create a library for UI components
add_library(KIOT_UIQT STATIC ${KIOT_UIQT_SOURCES})
if(Qt6Gui_VERSION VERSION_GREATER_EQUAL "6.10.0")
  find_package(Qt6GuiPrivate ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)
else()
  find_package(Qt6Gui ${QT_MIN_VERSION} CONFIG REQUIRED Private)
endif()
# Link libraries

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    Config
    CoreAddons
    DBusAddons
    Notifications
    IdleTime
    GlobalAccel
    KCMUtils
    I18n
    Solid
    BluezQt
    Service
    KIO
)

target_link_libraries(KIOT_UIQT PRIVATE
    KIOTShared
    Qt6::Core
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::Mqtt
    KF6::ConfigCore
    KF6::CoreAddons
)

# Export for parent project
target_include_directories(KIOT_UIQT PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)