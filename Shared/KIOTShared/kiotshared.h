// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "KIOTShared/kiotshared_export.h"

// Hjelpere og Transport
#include "platformhelper.h"
#include "dbusproperty.h"
#include "Transport/transportmanager.h"
#include "Transport/passwordstore.h"
#include "Config/configmanager.h"

// Entities
#include "entities/entity.h"
#include "entities/switch.h"
#include "entities/select.h"
#include "entities/number.h"
#include "entities/binarysensor.h"
#include "entities/button.h"
#include "entities/sensor.h"
#include "entities/text.h"
#include "entities/lock.h"
#include "entities/notify.h"
#include "entities/mediaplayer.h"
#include "entities/image.h"
#include "entities/camera.h"
#include "entities/update.h"
#include "entities/event.h"

// Plugins Interface
#include "Plugins/plugininterface.h"