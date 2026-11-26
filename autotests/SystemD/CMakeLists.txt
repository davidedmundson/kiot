# SPDX-FileCopyrightText: 2026 Thomas Kastenholz
# SPDX-License-Identifier: LGPL-2.1-or-later

# The SystemD plugin is a MODULE library, so the source under test is compiled in.
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS DBus)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Config)

kiot_add_test(systemdunitnametest systemdunitnametest.cpp ${PROJECT_SOURCE_DIR}/Plugins/SystemD/systemd_user.cpp
              LINK_LIBRARIES KIOTShared Qt6::DBus KF6::ConfigCore)
target_include_directories(systemdunitnametest PRIVATE ${PROJECT_SOURCE_DIR}/Plugins/SystemD)
