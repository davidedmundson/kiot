# SPDX-FileCopyrightText: 2026 Thomas Kastenholz
# SPDX-License-Identifier: LGPL-2.1-or-later

# Which notification URLs the Notifications plugin offers to open (header only).
kiot_add_test(notificationurltest notificationurltest.cpp LINK_LIBRARIES Qt6::Core)
target_include_directories(notificationurltest PRIVATE ${PROJECT_SOURCE_DIR}/Plugins/Notifications)
