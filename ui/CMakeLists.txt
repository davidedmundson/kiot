# KCM for kiot

kcmutils_add_qml_kcm(kcm_kiot SOURCES ${kcm_kiot_SRCS})

target_sources(kcm_kiot PRIVATE
    kcm_kiot.cpp
    ${kiot_kcfg_SRCS}
)

kconfig_target_kcfg_file(kcm_kiot
    FILE kiotsettings.kcfg
    CLASS_NAME KiotSettings
    GENERATE_MOC
    GENERATE_PROPERTIES
    MUTATORS
    NOTIFIERS
    PARENT_IN_CONSTRUCTOR
)


target_link_libraries(kcm_kiot PRIVATE
    Qt6::Core
    Qt6::Qml
    KF6::ConfigCore
    KF6::I18n
    KF6::KCMUtils
    KF6::KCMUtilsQuick
)