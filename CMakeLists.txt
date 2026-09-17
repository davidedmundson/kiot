cmake_minimum_required(VERSION 3.16)

# ------------------------------------------------------------------------------
# PROJECT INFO, sets the variables used in src to identify everything
# ------------------------------------------------------------------------------
project(kiot VERSION 0.0.1 DESCRIPTION "KDE Internet of Things desktop integration for Home Automation" HOMEPAGE_URL "https://github.com/davidedmundson/kiot" LANGUAGES CXX )

# ------------------------------------------------------------------------------
# PROSJEKT DEVELOPERS Shown under device info in HA with version and more
# ------------------------------------------------------------------------------
set(PROJECT_DEVELOPERS "David Edmundson & Odd Østlie" CACHE STRING "List of developers, add yourself here to be shown with the device info")


# ------------------------------------------------------------------------------
# Generates app_id/servicename from HOMEPAGE_URL
# ------------------------------------------------------------------------------
string(REGEX REPLACE "^https?://(www\\.)?" "" CLEAN_URL "${PROJECT_HOMEPAGE_URL}")
string(REGEX REPLACE "/.*$" "" CLEAN_DOMAIN "${CLEAN_URL}")

#Removed if to force match with app id since not a domain match
#if(NOT CLEAN_DOMAIN)
set(CLEAN_DOMAIN "davidedmundson.org")
#endif()

string(REPLACE "." ";" DOMAIN_PARTS "${CLEAN_DOMAIN}")
list(REVERSE DOMAIN_PARTS)
string(REPLACE ";" "." REVERSED_DOMAIN "${DOMAIN_PARTS}")
set(APP_ID "${REVERSED_DOMAIN}.${PROJECT_NAME}" CACHE STRING "Reverse-DNS App ID" FORCE)


string(TIMESTAMP CURRENT_YEAR "%Y")
set(CURRENT_YEAR ${CURRENT_YEAR} CACHE STRING "Current year")
# ------------------------------------------------------------------------------
# Adds project definition for reusability in sub directories cmakelists
# ------------------------------------------------------------------------------
add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")
add_definitions(-DPROJECT_DESCRIPTION="${PROJECT_DESCRIPTION}")
add_definitions(-DPROJECT_VERSION="${PROJECT_VERSION}")
add_definitions(-DPROJECT_DOMAIN="${PROJECT_HOMEPAGE_URL}")
add_definitions(-DAPP_ID="${APP_ID}")


message(STATUS "[CMake] Using project_name: PROJECT_NAME: ${PROJECT_NAME}")
message(STATUS "[CMake] Using project_version: PROJECT_VERSION: ${PROJECT_VERSION}")
message(STATUS "[CMake] Using project_description: PROJECT_DESCRIPTION: ${PROJECT_DESCRIPTION}")
message(STATUS "[CMake] Using project_developers: PROJECT_DEVELOPERS: ${PROJECT_DEVELOPERS}")
message(STATUS "[CMake] Using app_id: APP_ID: ${APP_ID}")

#USed to make sure we do it right with flatpak builds
option(BUILD_FLATPAK "Build for flatpak" OFF)

#IS this needed when we use the project part to set it?
#set(PROJECT_VERSION "0.1")
#set(PROJECT_VERSION_MAJOR 0)

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
    Quick
    QuickWidgets
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
    Service
    KIO
)

find_package(KF6 "1.7.0" REQUIRED COMPONENTS
    PulseAudioQt
)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

include(KDEClangFormat)
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h *.hpp *.c)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

set(SOURCES
    core/main.cpp
    core/core.cpp
    core/logging/messagehandler.cpp
    core/logging/messagehandler.h
    core/startup/startupmanager.cpp
    core/startup/startupmanager.h
    core/startup/systemdmanager.cpp
    core/startup/systemdmanager.h
    core/startup/desktopmanager.cpp
    core/startup/desktopmanager.h    
    core/startup/backgroundmanager.cpp
    core/startup/backgroundmanager.h    

)

# Auto generates dbus interfaces on build
set_source_files_properties(
   ./dbus/org.freedesktop.portal.Background.xml
   PROPERTIES
   NO_NAMESPACE ON
)
qt_add_dbus_interface(SOURCES ./dbus/org.freedesktop.portal.Background.xml backgroundportal)


add_subdirectory(Shared)
add_subdirectory(integrations)

# Add KCM if not building Flatpak
if(NOT BUILD_FLATPAK)
    add_subdirectory(kcm)
endif()

# Add qt ui if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/ui_qt/CMakeLists.txt")
    add_subdirectory(core/ui_qt)
endif()

# Add appdata if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/appdata/CMakeLists.txt")
    add_subdirectory(core/appdata)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

qt_add_qml_module(${PROJECT_NAME}
    URI main_qt.window
    RESOURCE_PREFIX /imports
    VERSION 1.0
    QML_FILES
        core/ui_qt/ui/main.qml
    RESOURCES
        core/ui_qt/resources.qrc
)
# Include resources for kwin script used by ActiveWindowWatcher
qt_add_resources(INTEGRATIONS_RESOURCES
    integrations/resources.qrc
)
target_sources(${PROJECT_NAME} PRIVATE 
    ${INTEGRATIONS_RESOURCES}

    )


target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Gui
    Qt6::Mqtt
    Qt6::DBus
    Qt6::Quick
    Qt6::QuickWidgets
    KF6::CoreAddons
    KF6::DBusAddons
    KIOTShared
    KIOT_UIQT
    KIOTIntegrations

  
)

#Adds the variables from cmakelist so its usable from cpp
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PROJECT_NAME="${PROJECT_NAME}"
    PROJECT_VERSION="${PROJECT_VERSION}"
    PROJECT_DESCRIPTION="${PROJECT_DESCRIPTION}"
    PROJECT_DOMAIN="${PROJECT_HOMEPAGE_URL}"
    PROJECT_DEVELOPERS="${PROJECT_DEVELOPERS}"
    APP_ID="${APP_ID}"
    CURRENT_YEAR="${CURRENT_YEAR}"
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
)

install(TARGETS ${PROJECT_NAME} ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

#ecm_install_configured_files(INPUT ${APP_ID}.desktop.in @ONLY DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
#ecm_install_configured_files(INPUT ${APP_ID}.desktop.in @ONLY DESTINATION ${KDE_INSTALL_APPDIR})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
