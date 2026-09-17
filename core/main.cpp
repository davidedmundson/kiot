#include "core/core.h"
#include "logging/messagehandler.h"
#include "ui_qt/mainwindow.h"

#include <QApplication>
#include <csignal>

#include <KAboutData>
#include <KDBusService>
#include <KSignalHandler>

DEFINE_LOGGER(main_cpp, Core.Main)
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
    QApplication::setDesktopFileName(PlatformHelper::generateServiceName());
    QApplication::setApplicationName(QStringLiteral(PROJECT_NAME));
    QApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    QApplication::setOrganizationName(QStringLiteral(PROJECT_NAME));
    QString domain = PlatformHelper::resolveOrganizationDomain(QStringLiteral(PROJECT_DOMAIN));
    QApplication::setOrganizationDomain( domain);
    QApplication app(argc, argv);

    initLogging();
    
    KAboutData aboutData(
        QStringLiteral(PROJECT_NAME),
        "KDE IOT",
        QStringLiteral(PROJECT_VERSION),
        QStringLiteral(PROJECT_DESCRIPTION),
        KAboutLicense::GPL_V3,
        "© 2024-"+QStringLiteral(CURRENT_YEAR)
    );
    
    KDBusService service(KDBusService::Unique | KDBusService::Replace);
    qCInfo(main_cpp) << "Starting" << PROJECT_NAME << "version:" << PROJECT_VERSION;
    MainWindow mainWindow;
    HaControl appControl;

    KSignalHandler::self()->watchSignal(SIGTERM);
    KSignalHandler::self()->watchSignal(SIGINT);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, [](int sig) {
        if (sig == SIGTERM || sig == SIGINT) {
            qCInfo(main_cpp) << "Shutting down" << QStringLiteral(PROJECT_NAME);
            QApplication::quit();
        }
    });

    return app.exec();
}
// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
