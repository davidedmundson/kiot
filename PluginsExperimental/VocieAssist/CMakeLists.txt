cmake_minimum_required(VERSION 3.31)

project(VoiceAssist 
    VERSION 0.0.1 
    DESCRIPTION "Template Plugin for KIOT" 
    HOMEPAGE_URL "https://github.com/davidedmundson/kiot" 
    LANGUAGES CXX 
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Make sure this follows the root cmakelists version
set(KF6_MIN_VERSION "6.15")
# Make sure this follows the root cmakelists version
set(QT_MIN_VERSION "6.6.0")
find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)

# Option to control whether we are building for the Flatpak sandbox
option(BUILD_FLATPAK_EXTENSION "Build as a Flatpak extension module" OFF)

message(STATUS "[CMake] Plugin: ${PROJECT_NAME} | Flatpak Mode: ${BUILD_FLATPAK_EXTENSION}")


# ---------------------------------------------------------------------------
# Standard x64 Protobuf generation via CMake's FindProtobuf
# ---------------------------------------------------------------------------
find_package(Protobuf REQUIRED)
message(STATUS "Found Protobuf: ${Protobuf_VERSION}")

set(PROTO_SRC_DIR  "${CMAKE_SOURCE_DIR}/proto")
set(PROTO_GEN_DIR  "${CMAKE_BINARY_DIR}/generated/proto")
file(MAKE_DIRECTORY "${PROTO_GEN_DIR}")

set(PROTO_FILES
    "${PROTO_SRC_DIR}/api_options.proto"
    "${PROTO_SRC_DIR}/api.proto"
)

set(PROTO_GEN_SRCS
    "${PROTO_GEN_DIR}/api_options.pb.cc"
    "${PROTO_GEN_DIR}/api.pb.cc"
)
set(PROTO_GEN_HDRS
    "${PROTO_GEN_DIR}/api_options.pb.h"
    "${PROTO_GEN_DIR}/api.pb.h"
)
# Legg til denne:
find_package(absl REQUIRED)
add_custom_command(
    OUTPUT  ${PROTO_GEN_SRCS} ${PROTO_GEN_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            --proto_path=${PROTO_SRC_DIR}
            --cpp_out=${PROTO_GEN_DIR}
            ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
    COMMENT "Generating ESPHome API protobuf C++ sources"
    VERBATIM
)

add_library(lva_proto OBJECT ${PROTO_GEN_SRCS})
set_target_properties(lva_proto PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(lva_proto PUBLIC ${PROTO_GEN_DIR} ${Protobuf_INCLUDE_DIRS})
target_link_libraries(lva_proto PUBLIC ${Protobuf_LIBRARIES})
target_compile_options(lva_proto PRIVATE
    -Wno-pedantic
    -Wno-unused-parameter
    -Wno-deprecated-declarations
)


# ---------------------------------------------------------------------------
# Find Dependencies via pkg-config / standard find_package
# ---------------------------------------------------------------------------
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(PULSE_SIMPLE REQUIRED IMPORTED_TARGET libpulse-simple)
pkg_check_modules(WEBRTC_APM REQUIRED IMPORTED_TARGET webrtc-audio-processing-1)
pkg_check_modules(LIBCURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(AVAHI_CLIENT REQUIRED IMPORTED_TARGET avahi-client)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)

# ---------------------------------------------------------------------------
# Vendored microfrontend + kissfft
# ---------------------------------------------------------------------------
set(MICROFRONTEND_DIR
    "${CMAKE_SOURCE_DIR}/third_party/microfrontend/include")

set(MICROFRONTEND_SOURCES
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/fft.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/fft_util.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/filterbank.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/filterbank_util.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/frontend.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/frontend_util.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/kiss_fft_int16.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/log_lut.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/log_scale.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/log_scale_util.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/noise_reduction.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/noise_reduction_util.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/pcan_gain_control.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/pcan_gain_control_util.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/window.cc
    ${MICROFRONTEND_DIR}/tensorflow/lite/experimental/microfrontend/lib/window_util.cc
)

add_library(lva_microfrontend OBJECT ${MICROFRONTEND_SOURCES})
set_target_properties(lva_microfrontend PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(lva_microfrontend PUBLIC
    ${MICROFRONTEND_DIR}
    ${MICROFRONTEND_DIR}/kissfft
)
target_compile_definitions(lva_microfrontend PUBLIC FIXED_POINT=16)
target_compile_options(lva_microfrontend PRIVATE
    -Wno-unused-parameter
    -Wno-unused-but-set-variable
    -Wno-unused-variable

)

# ==============================================================================
# 1. Paths and dependencies based on build mode
# ==============================================================================
if(BUILD_FLATPAK_EXTENSION)
    # Hardcoded path inside the sandbox for sharedlib
    include_directories(/app/include/kiotshared/)
endif()

# ==============================================================================
# 2. Target Definition & Sources
# ==============================================================================
add_library(${PROJECT_NAME} MODULE
    template.cpp
    template.h
    src/VoiceAssistantNode.cpp
    src/VoiceAssistantNode.h
    src/protocol/Frame.cpp
    src/protocol/MessageRegistry.cpp
    src/protocol/Connection.cpp
    src/protocol/ApiServer.cpp
    src/protocol/MdnsPublisher.cpp
    src/state/Preferences.cpp
    src/state/ServerState.cpp
    src/audio/LibMpvPlayer.cpp
    src/audio/PcmRingBuffer.cpp
    src/audio/AudioCapture.cpp
    src/audio/ExternalWakeWord.cpp
    src/audio/MicroFeatures.cpp
    src/audio/MicroWakeWord.cpp
    src/audio/OpenWakeWord.cpp
    src/audio/OpenWakeWordFeatures.cpp
    src/audio/TfliteRuntime.cpp
    src/audio/WakeWordEngine.cpp
    src/audio/WakeWordScanner.cpp
    src/audio/WebRtcProcessor.cpp
    src/satellite/Satellite.cpp
    src/tr/HomeButton.cpp
    src/tr/LedRing.cpp
    src/tr/MicMuteGpio.cpp
    src/tr/OtaClient.cpp
    src/tr/SendspinSignal.cpp
    src/tr/SoundConfWatcher.cpp
    src/tr/Supervisor.cpp
    src/tr/SupervisorHttpServer.cpp
    src/tr/SysInfo.cpp
    src/tr/SystemVolume.cpp
    src/tr/TimeZoneSync.cpp
    src/entities/MuteSwitchEntity.cpp
    src/entities/BinarySensorEntity.cpp
    src/entities/SensorEntity.cpp
    src/entities/TextEntity.cpp
    src/entities/LockEntity.cpp
    src/entities/ThinkingSoundEntity.cpp
    src/entities/NumberEntity.cpp
    src/entities/SelectEntity.cpp
    src/entities/MediaPlayerEntity.cpp
    src/entities/EventEntity.cpp
    src/entities/UpdateEntity.cpp
    src/entities/UpdateEntity_Ota.cpp
    src/util/Log.cpp

    )

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

# ==============================================================================


# ==============================================================================
# 3. Dependencies
# ==============================================================================
# Needed for KIOTShared
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS 
    Core
    Widgets
    Mqtt
)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS 
    Config

)

find_package(Threads REQUIRED)



# Finds our Shared lib on disk
find_package(KIOTShared REQUIRED)
message(STATUS "FINNER KIOTShared HER: ${KIOTShared_DIR}")
# Build target link libraries dynamically based on what was found
set(PLUGIN_LINK_LIBS 
    KIOTShared::KIOTShared
    Qt6::Core
    Qt6::Widgets
    Qt6::Mqtt           
    KF6::ConfigCore
        lva_proto
        lva_microfrontend
        nlohmann_json::nlohmann_json
        PkgConfig::MPV
        PkgConfig::PULSE_SIMPLE
        PkgConfig::ALSA
        PkgConfig::WEBRTC_APM
        PkgConfig::LIBCURL
        PkgConfig::AVAHI_CLIENT
        absl::log_internal_check_op
        absl::log_internal_message
        absl::raw_logging_internal
        Threads::Threads
        ${CMAKE_DL_LIBS}
    
    )

target_link_libraries(${PROJECT_NAME} PRIVATE ${PLUGIN_LINK_LIBS})

# ==============================================================================
# 4. Compile Definitions & Metadata
# ==============================================================================

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wno-deprecated-declarations
)
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
    # Flatpak expects the prefix to be /app, and installs to the plugins directory there
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/${PROJECT_NAME}
    )
else()
    # Native installation on the host system (e.g. Arch / Debian)
    set(PARENT_APP_NAME "kiot" CACHE STRING "Parent app name")
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/${PARENT_APP_NAME}/plugins
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    # Ensure it also outputs to the local bin directory during local test builds
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    )
endif()