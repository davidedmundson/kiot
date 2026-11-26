# SPDX-FileCopyrightText: 2026 Thomas Kastenholz
# SPDX-License-Identifier: LGPL-2.1-or-later

# The CustomSensor plugin is a MODULE library, so the source under test is compiled in.
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS CoreAddons)

kiot_add_test(customsensortest customsensortest.cpp ${PROJECT_SOURCE_DIR}/Plugins/CustomSensor/customsensor.cpp
              LINK_LIBRARIES KIOTShared KF6::CoreAddons)
target_include_directories(customsensortest PRIVATE ${PROJECT_SOURCE_DIR}/Plugins/CustomSensor)
