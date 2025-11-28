#include <QApplication>
#include <QDebug>

#include <KAboutData>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KDBusService>
#include <signal.h>
#include "core.h"

static void handleSigTerm(int s)
{
    signal(s, SIG_DFL);//IS this needed? a answer on google said it's supposed to Restore the default behavior after
    qApp->quit(); //Or would QApplication::quit(); be the propper way here?
}

int main(int argc, char **argv)
{
    //To many or just right?
    signal(SIGTERM, handleSigTerm);
    signal(SIGQUIT, handleSigTerm);
    signal(SIGINT, handleSigTerm);

    
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
