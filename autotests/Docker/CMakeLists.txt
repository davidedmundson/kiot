# SPDX-FileCopyrightText: 2026 Thomas Kastenholz
# SPDX-License-Identifier: LGPL-2.1-or-later

# The Docker plugin is a MODULE library, so the source under test is compiled in.
kiot_add_test(dockereventlistenertest dockereventlistenertest.cpp ${PROJECT_SOURCE_DIR}/Plugins/Docker/dockereventlistener.cpp
              LINK_LIBRARIES KIOTShared Qt6::Network)
target_include_directories(dockereventlistenertest PRIVATE ${PROJECT_SOURCE_DIR}/Plugins/Docker)
