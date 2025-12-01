#include <QApplication>
#include <QDebug>

#include "core.h"
#include <KAboutData>
#include <KConfigGroup>
#include <KDBusService>
#include <KSharedConfig>
#include <KSignalHandler>
// Is there a way to get the signal directly from
#include <csignal>
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    KAboutData aboutData(QStringLiteral("kiot"), "KDE IOT", QStringLiteral("0.1"), "KDE Internet of Things Connection", KAboutLicense::GPL_V3, "© 2024");
    KDBusService service(KDBusService::Unique);
    HaControl appControl;

    KSignalHandler::self()->watchSignal(SIGTERM);
    KSignalHandler::self()->watchSignal(SIGINT);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, [](int sig) {
        if (sig == SIGTERM || sig == SIGINT) {
            QApplication::quit();
        }
    });

    app.exec();
}
// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
