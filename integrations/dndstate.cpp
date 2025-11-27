// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "dbusproperty.h"
#include <QCoreApplication>

void setupDndSensor()
{
    auto sensor = new BinarySensor(qApp);
    sensor->setId("dnd");
    sensor->setName("Do not disturb");

    auto dnd = new DBusProperty("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Inhibited", qApp);
    QObject::connect(dnd, &DBusProperty::valueChanged, qApp, [sensor](const QVariant &value) {
        sensor->setState(value.toBool());
    });
    sensor->setState(dnd->value().toBool());

    // copy switch from nightmode
}

REGISTER_INTEGRATION("DnD",setupDndSensor,true)
