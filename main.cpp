#include "core.h"
#include "logging/messagehandler.h"

#include <KAboutData>
#include <KDBusService>
#include <KSignalHandler>
#include <QApplication>
#include <QDate>
#include <csignal>

/**
 * @brief Main entry point for the kiot application
 * @param argc Argument count
 * @param argv Argument vector
 * @return Application exit code
 *
 * Initializes the Qt application, sets up custom logging, handles KDE
 * integration, and watches for termination signals (SIGTERM, SIGINT).
 */
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
   // app.setApplicationName("org.davidedmundson.kiot");
  //  app.setApplicationDisplayName("Kiot");
    KAboutData aboutData(QStringLiteral("kiot"),
                         "KDE IOT",
                         QStringLiteral(KIOT_VERSION),
                         "KDE Internet of Things Connection",
                         KAboutLicense::GPL_V3,
                         "© 2024-" + QString::number(QDate::currentDate().year()));

    KDBusService service(KDBusService::Unique | KDBusService::Replace);
    initLogging();

    qCInfo(main_cpp) << "Starting kiot version " << QStringLiteral(KIOT_VERSION);
    
    HaControl appControl;

    KSignalHandler::self()->watchSignal(SIGTERM);
    KSignalHandler::self()->watchSignal(SIGINT);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, [](int sig) {
        if (sig == SIGTERM || sig == SIGINT) {
            qCInfo(main_cpp) << "Shutting down kiot";
            QApplication::quit();
        }
    });

    return app.exec();
}
// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
