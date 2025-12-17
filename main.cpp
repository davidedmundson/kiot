#include "core.h"
#include "logging/messagehandler.h"

#include <QApplication>
#include <csignal>

#include <KAboutData>
#include <KDBusService>
#include <KSignalHandler>

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
    
    // Initialize logging system with configuration
    initLogging();
    
    // Set up KDE application metadata
    KAboutData aboutData(
        QStringLiteral("kiot"),
        "KDE IOT",
        QStringLiteral("0.1"),
        "KDE Internet of Things Connection",
        KAboutLicense::GPL_V3,
        "© 2024"
    );
    
    // Set up D-Bus service
    KDBusService service(KDBusService::Unique | KDBusService::Replace);
    
    // Initialize application control
    HaControl appControl;

    // Set up signal handling for graceful shutdown
    KSignalHandler::self()->watchSignal(SIGTERM);
    KSignalHandler::self()->watchSignal(SIGINT);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, [](int sig) {
        if (sig == SIGTERM || sig == SIGINT) {
            qCInfo(main_cpp) << "Shutting down kiot";
            QApplication::quit();
        }
    });

    // Start the application event loop
    return app.exec();
}
// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
