#include "desktopmanager.h"
#include "Shared/platformhelper.h"
#include "core/core.h"

#include <QObject>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>


DEFINE_LOGGER(dm, Core.Startup.DesktopManager)

DesktopManager::DesktopManager(QObject *parent) : QObject(parent) {}

QString DesktopManager::desktopFilePath() {
    if(PlatformHelper::isFlatpak())
    {
        return QDir::homePath() + "/.config/autostart/" + PlatformHelper::generateServiceName() + ".desktop";
    }
    
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString path = configDir + "/autostart/";
    QDir dir(path);
    if (!dir.exists()) {
        if (!dir.mkpath(path)) {
            qCWarning(dm) << "Failed to create directory:" << path;
        }
    }
    return path + PlatformHelper::generateServiceName() + ".desktop";
}

QString DesktopManager::desktopFileContent() {
    QString execLine;
     QString executablePath;
    if(PlatformHelper::isFlatpak())
    {
        executablePath =  "/usr/bin/flatpak run --command=" + QString(PROJECT_NAME) + " " + PlatformHelper::generateServiceName();
        qCDebug(dm) << executablePath;
        execLine = "Exec=" + executablePath;
    
    }
    else{
        executablePath = QCoreApplication::applicationFilePath();   
        qCDebug(dm) << executablePath;
        execLine = "Exec=" + executablePath;
    }
    return QStringLiteral(
        "[Desktop Entry]\n"
        "Name=%1\n"
        "Comment=%2\n"
        "Icon=%3\n"
        "Type=Application\n"
        "Categories=Utility;\n"
        "X-KDE-StartupNotify=false\n"
        "NoDisplay=false\n"
        "%4\n"
        "\n"
        "# Autostart specific\n"
        "X-KDE-autostart-phase=2\n"
        "X-KDE-autostart-after=panel\n"
        "OnlyShowIn=KDE;\n"
    ).arg(QStringLiteral(PROJECT_NAME))
     .arg(QStringLiteral(PROJECT_DESCRIPTION))
     .arg(QStringLiteral(APP_ID)) // Eller app-id hvis du har det definert
     .arg(execLine);
}

bool DesktopManager::writeDesktopFile() {
    QFile file(desktopFilePath());
    QDir().mkpath(QFileInfo(file).absolutePath());

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << desktopFileContent();
        file.close();
        return true;
    }

    qCWarning(dm) << "Failed to write desktop file:" << file.errorString();
    return false;
}

bool DesktopManager::removeDesktopFile() {
    QFile file(desktopFilePath());
    if (file.exists()) {
        return file.remove();
    }
    return true;
}

// Sett opp autostart for .desktop (ingen D-Bus kreves her)
bool DesktopManager::setupAutostart(bool enabled) {
    qCDebug(dm) << "Setting .desktop autostart to:" << enabled;

    if (enabled) {
        qCDebug(dm) << "Writing desktop file to:" << desktopFilePath();
        if (!writeDesktopFile()) {
            qCWarning(dm) << "Failed to write desktop file";
            return false;
        }
        return true;
    } else {
        qCDebug(dm) << "Removing desktop file";
        bool success = removeDesktopFile();
        if (!success) {
            qCWarning(dm) << "Desktop file already removed or couldn't be removed";
        }
        return success;
    }
}

// Sjekker om filen eksisterer og om Exec-linjen matcher nåværende binærsti
bool DesktopManager::isAutostartEnabled()
{
    // 1. Sjekk at desktop-filen eksisterer
    QFile file(desktopFilePath());
    if (!file.exists()) {
        qCDebug(dm) << "Desktop file missing:" << desktopFilePath();
        return false;
    }

    // 2. Sjekk at Exec matcher nåværende sti
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        QString execLine;
        QString executablePath;
        if(PlatformHelper::isFlatpak())
        {
            executablePath =  "/usr/bin/flatpak run --command=" + QString(PROJECT_NAME) + " " + PlatformHelper::generateServiceName();
            execLine = "Exec=" + executablePath;
        }
        else{
            executablePath = QCoreApplication::applicationFilePath();   
            execLine = "Exec=" + executablePath;
        }
        QString expectedExec = execLine;

        if (!content.contains(expectedExec)) {
            qCDebug(dm) << "Desktop file Exec mismatch for current path";
            return false;
        }
    } else {
        qCDebug(dm) << "Failed to open desktop file for reading:" << file.errorString();
        return false;
    }

    // Alt OK
    return true;
}