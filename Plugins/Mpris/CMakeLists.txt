cmake_minimum_required(VERSION 3.31)

project(Mpris VERSION 0.0.1 DESCRIPTION "Short Plugin Description" HOMEPAGE_URL "https://github.com/davidedmundson/kiot" LANGUAGES CXX )

set(CMAKE_AUTOMOC ON)

message(STATUS "[CMake] Plugin: ${PROJECT_NAME}")

set(DBUS_XML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dbus")

# Samler kildene akkurat som i din fungerende integrasjonskode
set(MPRIS_SRCS
    mprisplugin.cpp
    mprisplugin.h
    abstractplayercontainer.h
    abstractplayercontainer.cpp
    playercontainer.h
    playercontainer.cpp
    metadatafallback.h
    mprismultiplexer.h
    mprismultiplexer.cpp
    mprismultiplexer.h
    mprismultiplexer.cpp


)

set_source_files_properties(
    ${DBUS_XML_DIR}/org.freedesktop.DBus.Properties.xml
    ${DBUS_XML_DIR}/org.mpris.MediaPlayer2.Player.xml
    ${DBUS_XML_DIR}/org.mpris.MediaPlayer2.xml
    PROPERTIES
    NO_NAMESPACE ON
)

qt_add_dbus_interface(MPRIS_SRCS ${DBUS_XML_DIR}/org.freedesktop.DBus.Properties.xml dbusproperties)
qt_add_dbus_interface(MPRIS_SRCS ${DBUS_XML_DIR}/org.mpris.MediaPlayer2.Player.xml mprisplayer)
qt_add_dbus_interface(MPRIS_SRCS ${DBUS_XML_DIR}/org.mpris.MediaPlayer2.xml mprisroot)

add_library(${PROJECT_NAME} MODULE ${MPRIS_SRCS})

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    DBus
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    KIOTShared
    Qt6::Core
    Qt6::DBus
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLUGIN_NAME=\"${PROJECT_NAME}\"
    PLUGIN_VERSION=\"${PROJECT_VERSION}\"
    PLUGIN_DESCRIPTION=\"${PROJECT_DESCRIPTION}\"
    PLUGIN_DOMAIN=\"${PROJECT_HOMEPAGE_URL}\"
    CURRENT_YEAR=\"${CURRENT_YEAR}\"
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/${PARENT_APP_NAME}/plugins
    RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}
)