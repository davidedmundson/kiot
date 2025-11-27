// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include "login1_manager_interface.h"

void setupSuspend()
{
    {
        Button *button = new Button(qApp);
        button->setId("suspend");
        button->setName("Suspend");

        QObject::connect(button, &Button::triggered, qApp, []() {
            OrgFreedesktopLogin1ManagerInterface logind(QStringLiteral("org.freedesktop.login1"),
                                                        QStringLiteral("/org/freedesktop/login1"),
                                                        QDBusConnection::systemBus());
            logind.Suspend(false).waitForFinished();
        });
    }

    {
        Button *button = new Button(qApp);
        button->setId("hibernate");
        button->setName("Hibernate");

        QObject::connect(button, &Button::triggered, qApp, []() {
            OrgFreedesktopLogin1ManagerInterface logind(QStringLiteral("org.freedesktop.login1"),
                                                        QStringLiteral("/org/freedesktop/login1"),
                                                        QDBusConnection::systemBus());
            logind.Hibernate(false).waitForFinished();
        });
    }

    {
        Button *button = new Button(qApp);
        button->setId("poweroff");
        button->setName("Poweroff");

        QObject::connect(button, &Button::triggered, qApp, []() {
            OrgFreedesktopLogin1ManagerInterface logind(QStringLiteral("org.freedesktop.login1"),
                                                        QStringLiteral("/org/freedesktop/login1"),
                                                        QDBusConnection::systemBus());
            logind.PowerOff(false).waitForFinished();
        });
    }
}

REGISTER_INTEGRATION("PowerController",setupSuspend,true)
