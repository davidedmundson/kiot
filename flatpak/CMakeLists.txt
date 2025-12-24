# Flatpak-specific installation
# This CMakeLists.txt handles installation of Flatpak-specific files

# Install the icon (scalable SVG)
install(
  FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.davidedmundson.kiot.svg
  DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps
)
install(
    
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/generated-icons/hicolor/
  DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor
  FILES_MATCHING PATTERN "*.png"
)

# Use the generate-icons.sh script to create png icons, then install with:
# install(
#   FILES ${CMAKE_CURRENT_SOURCE_DIR}/generated-icons/hicolor/16x16/apps/org.davidedmundson.kiot.png
#   DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/16x16/apps
# )
# Repeat for other sizes: 22x22, 24x24, 32x32, 48x48, 64x64, 128x128, 256x256


install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.davidedmundson.kiot.metainfo.xml
    DESTINATION ${KDE_INSTALL_METAINFODIR}
)


# TODO make a flatpak only menu option in systray to open autostart in host
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.davidedmundson.kiot.desktop
    DESTINATION ${KDE_INSTALL_AUTOSTARTDIR}
)


install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.davidedmundson.kiot.desktop
    DESTINATION ${KDE_INSTALL_APPDIR}
)


# Print installation summary
message(STATUS "Flatpak files will be installed to:")
message(STATUS "  Icons: ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps/org.davidedmundson.kiot.svg")
message(STATUS "  Metainfo: ${KDE_INSTALL_METAINFODIR}/org.davidedmundson.kiot.metainfo.xml")
message(STATUS "  Autostart: ${KDE_INSTALL_AUTOSTARTDIR}/org.davidedmundson.kiot.desktop")
message(STATUS "  Applications: ${KDE_INSTALL_APPDIR}/org.davidedmundson.kiot.desktop")

if(BUILD_FLATPAK)
    message(STATUS "Building for Flatpak environment")
endif()