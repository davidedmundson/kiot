cmake_minimum_required(VERSION 3.31)

project(Notification VERSION 0.0.1 DESCRIPTION "Lets you send notifications from home assistant to KDE" HOMEPAGE_URL "https://github.com/davidedmundson/kiot" LANGUAGES CXX )

set(CMAKE_AUTOMOC ON)


message(STATUS "[CMake] Plugin: ${PROJECT_NAME}")

add_library(${PROJECT_NAME} MODULE
    notificationplugin.cpp
    notificationplugin.h
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})


find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    Notifications

)

find_package(Qt6 REQUIRED COMPONENTS Gui)

target_link_libraries(${PROJECT_NAME} PRIVATE
    KIOTShared
    KF6::Notifications
    KF6::CoreAddons
    Qt6::Gui
)

# Plasser den ferdige .so-filen direkte i plugins-katalogen der Core forventer å finne den
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
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
configure_file(kiot.notifyrc.in ${CMAKE_CURRENT_BINARY_DIR}/kiot.notifyrc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kiot.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
