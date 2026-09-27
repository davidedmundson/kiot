cmake_minimum_required(VERSION 3.20)
include(FetchContent)
project(SendSpin 
    VERSION 0.0.1 
    DESCRIPTION "A KIOT Plugin that implements a sendspin client to use your KDE setup with music assistant" 
    HOMEPAGE_URL "https://github.com/davidedmundson/kiot" 
    LANGUAGES CXX 
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Grabs the sendspin-cpp repo and makes it usable for our plugin
FetchContent_Declare(
    sendspin
    GIT_REPOSITORY https://github.com/Sendspin/sendspin-cpp.git
    GIT_TAG        main 
    GIT_SHALLOW    FALSE    
)
FetchContent_MakeAvailable(sendspin)

set(CMAKE_AUTOMOC ON)
# Versjonskrav
set(KF6_MIN_VERSION "6.15")
set(QT_MIN_VERSION "6.6.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)







# Option for Flatpak
option(BUILD_FLATPAK_EXTENSION "Build as a Flatpak extension module" OFF)
message(STATUS "[CMake] Plugin: ${PROJECT_NAME} | Flatpak Mode: ${BUILD_FLATPAK_EXTENSION}")

# ==============================================================================
# 1. Paths and dependencies based on build mode
# ==============================================================================
if(BUILD_FLATPAK_EXTENSION)
    include_directories(/app/include/kiotshared/)
endif()

# ==============================================================================
# 2. Target Definition & Sources
# (Husk å endre til dine faktiske filnavn hvis du ikke bruker template.cpp lenger)
# ==============================================================================
add_library(${PROJECT_NAME} MODULE
    sendspinplugin.cpp
    sendspinplugin.h
    sendspinclient.h
    sendspinclient.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

# ==============================================================================
# 3. Dependencies
# ==============================================================================

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS 
    Core
    Widgets
    Mqtt
)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS 
    Config
)

find_package(KIOTShared REQUIRED)
pkg_check_modules(PULSE REQUIRED libpulse libpulse-simple)
target_include_directories(${PROJECT_NAME} PRIVATE ${PULSE_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    KIOTShared::KIOTShared
    Qt6::Core
    Qt6::Widgets
    Qt6::Mqtt    
    KF6::ConfigCore
    Threads::Threads
    sendspin
    ${PULSE_LIBRARIES}
)

# ==============================================================================
# 4. Compile Definitions & Metadata
# ==============================================================================
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PROJECT_NAME="kiot"
    PLUGIN_NAME="${PROJECT_NAME}"
    PLUGIN_VERSION="${PROJECT_VERSION}"
    PLUGIN_DESCRIPTION="${PROJECT_DESCRIPTION}"
    PLUGIN_DOMAIN="${PROJECT_HOMEPAGE_URL}"
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
)

# ==============================================================================
# 5. Smart Installation Rules
# ==============================================================================
if(BUILD_FLATPAK_EXTENSION)
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/${PROJECT_NAME}
    )
else()
    set(PARENT_APP_NAME "kiot" CACHE STRING "Parent app name")
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/${PARENT_APP_NAME}/plugins
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    )
endif()