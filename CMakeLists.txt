cmake_minimum_required(VERSION 3.16)

# Moved version here to work with github actions and it looks like this auto generates PROJECT_VERSION and all that
project(kiot VERSION 0.0.1)
# For automatic firmware updates in connected node
add_definitions(-DKIOT_VERSION="${PROJECT_VERSION}")

set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "6.15")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMOptionalAddSubdirectory)
include(FeatureSummary)
include(KDEClangFormat)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(ECMQmlModule)
include(ECMConfiguredInstall)


find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    DBus
    Widgets
    Mqtt
)

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
)

find_package(KF6 "1.7.0" REQUIRED COMPONENTS
    PulseAudioQt
)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

include(KDEClangFormat)
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h *.hpp *.c)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

set(SOURCES
    main.cpp
    core.cpp
    dbusproperty.cpp
    dbusproperty.h
    systray.cpp
    entities/entity.cpp
    entities/binarysensor.cpp
    entities/button.cpp
    entities/camera.cpp
    entities/mediaplayer.cpp
    entities/notify.cpp
    entities/number.cpp
    entities/switch.cpp
    entities/event.cpp
    entities/select.cpp
    entities/sensor.cpp
    entities/text.cpp
    entities/lock.cpp
    entities/update.cpp
    logging/messagehandler.cpp
    logging/messagehandler.h
)

add_subdirectory(integrations)
add_subdirectory(ui)

add_executable(kiot ${SOURCES})

target_link_libraries(
    kiot
    Qt6::Gui
    Qt6::Mqtt
    Qt6::DBus
    KIOTIntegrations
    KF6::CoreAddons
    KF6::DBusAddons
)
target_compile_definitions(kiot PRIVATE
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
)

install(TARGETS kiot ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

ecm_install_configured_files(INPUT org.davidedmundson.kiot.desktop.in @ONLY DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
ecm_install_configured_files(INPUT org.davidedmundson.kiot.desktop.in @ONLY DESTINATION ${KDE_INSTALL_APPDIR})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
