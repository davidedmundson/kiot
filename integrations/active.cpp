// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/binarysensor.h"
#include <KIdleTime>
#include <QCoreApplication>

void setupActiveSensor()
{
    auto sensor = new BinarySensor(qApp);
    sensor->setId("active");
    sensor->setName("Active");
    sensor->setDiscoveryConfig("device_class", "presence");

    // mark as idle after 1 minute. Then in HA to detect a 5 minute idle-ness, wait till we're in this state for 4 minutes
    auto kidletime = KIdleTime::instance();
    auto id = kidletime->addIdleTimeout(60 * 1000);
    QObject::connect(kidletime, &KIdleTime::resumingFromIdle, qApp, [sensor]() {
        sensor->setState(true);
    });
    QObject::connect(kidletime, &KIdleTime::timeoutReached, qApp, [id, kidletime, sensor](int _id) {
        if (_id != id) {
            return;
        }
        sensor->setState(false);
        kidletime->catchNextResumeEvent();
    });
    sensor->setState(true);
}

REGISTER_INTEGRATION("Active", setupActiveSensor, true)
