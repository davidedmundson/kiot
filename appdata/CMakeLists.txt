# appdata specific installation
# This CMakeLists.txt handles installation of appdata-specific files

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.svg")
    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps
    )
endif()


if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/generated-icons/hicolor")
    message(STATUS "[ICONS] Generating missing icons based on svg template")
    execute_process(
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/generate-icons.sh" "${APP_ID}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE ICON_RESULT
    )
endif()
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/generated-icons/hicolor")
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/generated-icons/hicolor/
        DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor
        FILES_MATCHING PATTERN "*.png"
    )
endif()



#Flatpak gave me so many errors with ecm_install_configured_files 
#I had to split this and use a static metainfo and .desktop file to get it working
#TODO Figure out the best way here
if(BUILD_FLATPAK)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.metainfo.xml.flatpak")
    install(
      FILES ${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.metainfo.xml.flatpak
      RENAME ${APP_ID}.metainfo.xml
      DESTINATION ${KDE_INSTALL_METAINFODIR}
    )
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.desktop.flatpak")
    install(
      FILES ${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.desktop.flatpak 
      RENAME ${APP_ID}.desktop
      DESTINATION ${KDE_INSTALL_APPDIR}/
    )
  endif()
else()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.metainfo.xml.in")
      ecm_install_configured_files(INPUT ${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.metainfo.xml.in @ONLY DESTINATION ${KDE_INSTALL_METAINFODIR})
  endif()

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.desktop.in")
    ecm_install_configured_files(INPUT ${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.desktop.in @ONLY DESTINATION ${KDE_INSTALL_APPDIR})
    ecm_install_configured_files(INPUT ${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.desktop.in @ONLY DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
  endif()
endif()





