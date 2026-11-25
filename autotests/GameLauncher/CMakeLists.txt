# SPDX-FileCopyrightText: 2026 Thomas Kastenholz
# SPDX-License-Identifier: LGPL-2.1-or-later

# The GameLauncher plugin is a MODULE library, so the sources under test are compiled in;
# gamebase.h is listed for its moc.
set(launchers ${PROJECT_SOURCE_DIR}/Plugins/GameLauncher/launchers)
kiot_add_test(lutrisscannertest lutrisscannertest.cpp ${launchers}/LutrisScanner.cpp ${launchers}/gamebase.h
              LINK_LIBRARIES KIOTShared)
target_include_directories(lutrisscannertest PRIVATE ${launchers})
