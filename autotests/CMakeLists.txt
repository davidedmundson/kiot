# SPDX-FileCopyrightText: 2026 Thomas Kastenholz
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Unit and integration tests, run with `ctest` from the build directory (.github/workflows/tests.yml
# runs them on every push). Every test gets its own temporary XDG directories and a D-Bus session
# address that leads nowhere (testenv.h), so none of them reads or writes ~/.config/kiotrc or
# reaches the desktop session.
#
# mqttentitiestest needs the mosquitto broker on PATH or in /usr/sbin; without it the test skips,
# unless KIOT_MQTT_TESTS=required makes that a failure (as in CI).
#
# A plugin's tests go in a directory of their own, autotests/<plugin>/CMakeLists.txt, which is picked
# up below; it calls kiot_add_test() for each test, so branches adding tests don't touch this file.

include(ECMAddTests)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Test Mqtt Network)

# kiot_add_test(<name> <sources>... LINK_LIBRARIES <libs>...)
# A QtTest program <name> from <sources>, with testenv.h on the include path and the test environment.
function(kiot_add_test name)
    cmake_parse_arguments(ARG "" "" "LINK_LIBRARIES" ${ARGN})
    ecm_add_test(${ARG_UNPARSED_ARGUMENTS} TEST_NAME ${name} LINK_LIBRARIES Qt6::Test ${ARG_LINK_LIBRARIES})
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/autotests)
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
endfunction()

foreach(test platformhelpertest configmanagertest entitytest mqttentitiestest)
    kiot_add_test(${test} ${test}.cpp LINK_LIBRARIES KIOTShared Qt6::Mqtt Qt6::Network)
endforeach()

# The text VDF parser of the GameLauncher plugin is compiled in directly: plugins are MODULE
# libraries, which a test can't link against.
kiot_add_test(vdfparsertest vdfparsertest.cpp ${PROJECT_SOURCE_DIR}/Plugins/GameLauncher/helpers/VdfParser.cpp
              LINK_LIBRARIES Qt6::Core)
target_include_directories(vdfparsertest PRIVATE ${PROJECT_SOURCE_DIR}/Plugins/GameLauncher)

file(GLOB plugin_test_dirs LIST_DIRECTORIES true CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(dir ${plugin_test_dirs})
    if(EXISTS "${dir}/CMakeLists.txt")
        add_subdirectory(${dir})
    endif()
endforeach()
