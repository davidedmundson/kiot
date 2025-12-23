# Install the icon
install(
  FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.davidedmundson.kiot.svg
  DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps
)
# Install the metainfo file
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.davidedmundson.kiot.metainfo.xml
    DESTINATION ${KDE_INSTALL_METAINFODIR}
)

# Install the desktop file
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.davidedmundson.kiot.desktop
    DESTINATION ${KDE_INSTALL_AUTOSTARTDIR}
)


install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.davidedmundson.kiot.desktop
    DESTINATION ${KDE_INSTALL_APPDIR}
)
