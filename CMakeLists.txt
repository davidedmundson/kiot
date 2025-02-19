cmake_minimum_required(VERSION 3.16)

project(kiot)
set(PROJECT_VERSION "6.0.80")
set(PROJECT_VERSION_MAJOR 6)

set(PROJECT_DEP_VERSION "6.0.80")
set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "5.240.0")
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

set(KF6_MIN_VERSION "5.240.0")
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    Config
    CoreAddons
    DBusAddons
    Notifications
    IdleTime
    GlobalAccel
)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(SOURCES main.cpp core.cpp dbusproperty.h dbusproperty.cpp)

add_subdirectory(integrations)

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

install(TARGETS kiot ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

ecm_install_configured_files(INPUT org.davidedmundson.kiot.desktop.in @ONLY DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
ecm_install_configured_files(INPUT org.davidedmundson.kiot.desktop.in @ONLY DESTINATION ${KDE_INSTALL_APPDIR})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
