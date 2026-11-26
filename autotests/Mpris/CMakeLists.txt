# SPDX-FileCopyrightText: 2026 Thomas Kastenholz
# SPDX-License-Identifier: LGPL-2.1-or-later

# The track/album fallbacks of the Mpris plugin, from xesam:url (header only).
kiot_add_test(metadatafallbacktest metadatafallbacktest.cpp LINK_LIBRARIES Qt6::Core)
target_include_directories(metadatafallbacktest PRIVATE ${PROJECT_SOURCE_DIR}/Plugins/Mpris)
