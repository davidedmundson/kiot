# KCM for kiot


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

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    DBus
)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    Config
    CoreAddons
    KCMUtils
    I18n
)


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
    Qt6::DBus
    KF6::CoreAddons
    KIOTShared::KIOTShared
    KF6::ConfigCore
    KF6::I18n
    KF6::KCMUtils
    KF6::KCMUtilsQuick
)

