//TODO ask about the best way to make sure we get a clean shutdown 
//so the deconstructors of the integrations can run
#include <QApplication>
#include <QDebug>

#include <KAboutData>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KDBusService>
#include <signal.h>
#include "core.h"

static void handleSigTerm(int)
{
    QApplication::quit();
}

int main(int argc, char **argv)
{
    signal(SIGTERM, handleSigTerm);

    QApplication app(argc, argv);

    KAboutData aboutData(
        QStringLiteral("kiot"),
        "KDE IOT",
        QStringLiteral("0.1"),
        "KDE Internet of Things Connection",
        KAboutLicense::GPL_V3,
        "© 2024");

    KDBusService service(KDBusService::Unique);

    HaControl appControl;

    return app.exec();
}
// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
