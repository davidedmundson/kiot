cmake_minimum_required(VERSION 3.31)

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
set(PARENT_APP_NAME ${PROJECT_NAME})

# ------------------------------------------------------------------------------
# Prints the project information
# ------------------------------------------------------------------------------
message(STATUS "[CMake] Using project_name: PROJECT_NAME: ${PROJECT_NAME}")
message(STATUS "[CMake] Using project_version: PROJECT_VERSION: ${PROJECT_VERSION}")
message(STATUS "[CMake] Using project_description: PROJECT_DESCRIPTION: ${PROJECT_DESCRIPTION}")
message(STATUS "[CMake] Using project_developers: PROJECT_DEVELOPERS: ${PROJECT_DEVELOPERS}")
message(STATUS "[CMake] Using app_id: APP_ID: ${APP_ID}")

# ------------------------------------------------------------------------------
# Adds build options to make enable/disable features simple
# ------------------------------------------------------------------------------
option(BUILD_FLATPAK "Build for flatpak" OFF)
option(BUILD_QTUI "Include the qt ui in core" ON)
option(BUILD_APPDATA "Include the appdata in core" ON)
option(BUILD_PLUGINS "Build every plugin project in the Plugins directory" ON)
option(BUILD_EXPERIMENTAL_PLUGINS "Build every experimental plugin project in the PluginsExperimental directory" OFF)
#USed to build the Qt UI if the cmakelists exitss
set(HAS_QTUI EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/ui_qt/CMakeLists.txt")

# ------------------------------------------------------------------------------
# Adds project definition for packaging builds into difrent systems
# ------------------------------------------------------------------------------
set(CPACK_GENERATOR "DEB;RPM;AppImage;TGZ")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_MAINTAINER "${PROJECT_DEVELOPERS}")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PROJECT_DEVELOPERS}")
#DEB check for dependency building
find_program(DPKG_SHLIBDEPS_EXECUTABLE NAMES dpkg-shlibdeps)
if(DPKG_SHLIBDEPS_EXECUTABLE)
    message(STATUS "[CMake] Using dpkg-shlibdeps for DEB dependencies.")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
else()
    message(STATUS "[CMake] Using manual list for DEB dependencies.")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, qt6-base-common, qt6-mqtt, libkf6configcore, libkf6coreaddons, libkf6dbusaddons")
endif()

#IS this needed when we use the project part to set it?
#set(PROJECT_VERSION "0.1")
#set(PROJECT_VERSION_MAJOR 0)

set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "6.15")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")

set(CMAKE_CXX_STANDARD 20)
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

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

#Originale gives error with Experimental because sendspin-cpp uses its own rules here
#file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h *.hpp *.c)
#Changed to specific folders to avoid formatting the PluginsExperimental folder as its a mess
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shared/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shared/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shared/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shared/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/kcm/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/kcm/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/kcm/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/kcm/*.c"
)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

# Builds the shared lib needed by everything else
add_subdirectory(Shared)

# Builds every plugin in the Plugins folder on loop
if(BUILD_PLUGINS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/CMakeLists.txt")
        add_subdirectory(Plugins)
    else()
        message(STATUS "[CMake] Plugins subdir missing Cmakelists.......PROBLEMPROBLEMPROBLEM")
    endif()
endif()

# Builds the kcm module if not in a flatpak sandbox
if(NOT BUILD_FLATPAK)
    add_subdirectory(kcm)
endif()

# Builds the core binary
add_subdirectory(core)



# BUilds every Experimental plugin if you set it to ON
if(BUILD_EXPERIMENTAL_PLUGINS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/PluginsExperimental/CMakeLists.txt")
        add_subdirectory(PluginsExperimental)
    else()
        message(STATUS "[CMake] PluginsExperimental subdir missing CMakeLists.......PROBLEMPROBLEMPROBLEM")
    endif()
endif()


#feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)


include(CPack)