
find_package(PkgConfig REQUIRED)

include_directories(..)

add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")
add_definitions(-DPROJECT_DESCRIPTION="${PROJECT_DESCRIPTION}")
add_definitions(-DPROJECT_VERSION="${PROJECT_VERSION}")
add_definitions(-DPROJECT_DOMAIN="${PROJECT_HOMEPAGE_URL}")
add_definitions(-DAPP_ID="${APP_ID}")
add_definitions(-DPROJECT_DEVELOPERS="${PROJECT_DEVELOPERS}")

set(KIOT_SHARED_SRC
    
    dbusproperty.cpp
    dbusproperty.h
    entities/entity.cpp
    entities/binarysensor.cpp
    entities/button.cpp
    entities/number.cpp
    entities/switch.cpp
    entities/event.cpp
    entities/select.cpp
    entities/sensor.cpp
    entities/text.cpp
    entities/lock.cpp
    entities/notify.cpp
    entities/mediaplayer.cpp
    platformhelper.cpp
    platformhelper.h
)


add_library(KIOTShared OBJECT ${KIOT_SHARED_SRC})


target_link_libraries(KIOTShared
    KF6::CoreAddons
    KF6::ConfigCore
    Qt6::Widgets
    Qt6::Mqtt
    Qt6::DBus

)

target_include_directories(KIOTShared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)




