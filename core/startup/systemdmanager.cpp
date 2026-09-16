#include "systemdmanager.h"
#include "Shared/platformhelper.h"
#include "core/core.h"

#include <QObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QGuiApplication>
Q_DECLARE_LOGGING_CATEGORY(sm)
Q_LOGGING_CATEGORY(sm, LOG_CAT(SystemdManager))

SystemdManager::SystemdManager(QObject *parent) : QObject(parent) {}

QString SystemdManager::serviceFilePath() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString path = configDir + "/systemd/user/";
    QDir dir(path);
    if (!dir.exists()) {
        if (!dir.mkpath(path)) {
            qCWarning(sm) << "Failed to create directory:" << path;
        }
    }
    return path + QString(PROJECT_NAME) + ".service";
}

QString SystemdManager::serviceContent() {
    QString execLine;
    QString executablePath = QCoreApplication::applicationFilePath();   
    qCDebug(sm) << executablePath;
    execLine = "ExecStart=" + executablePath;
    
    return QStringLiteral(
        "[Unit]\n"
        "Description=%1\n"
        "Documentation=%2\n"
        "Wants=network-online.target\n"
        "After=network-online.target graphical-session.target\n"
        "\n"
        "[Service]\n"
        "Type=simple\n"
        "%3\n"
        "Restart=on-failure\n"
        "RestartSec=3\n"
        "\n"
        "Slice=user.slice\n"
        "\n"
        "[Install]\n"
        "WantedBy=graphical-session.target\n"
    ).arg(QStringLiteral(PROJECT_DESCRIPTION))
     .arg(QStringLiteral(PROJECT_DOMAIN))
     .arg(execLine);
}

bool SystemdManager::writeServiceFile() {
    QFile file(serviceFilePath());
    QDir().mkpath(QFileInfo(file).absolutePath());

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << serviceContent();
        file.close();
        return true;
    }

    qCWarning(sm) << "Failed to write service file:" << file.errorString();
    return false;
}

bool SystemdManager::removeServiceFile() {
    QFile file(serviceFilePath());
    return file.remove();
}

// Originale D-Bus kall med riktig signatur
bool SystemdManager::enableServiceViaDBus() {

    QDBusInterface systemd("org.freedesktop.systemd1", "/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager",QDBusConnection::sessionBus());

    if (!systemd.isValid()) {
        qCWarning(sm) << "Cannot connect to systemd via D-Bus";
        return false;
    }

    QDBusReply<bool> enableReply = systemd.call(
        "EnableUnitFiles",
        QStringList() << QString(PROJECT_NAME) + ".service",
        false, // runtime
        true   // force
    );

    if (!enableReply.isValid()) {
        qCWarning(sm) << "Failed to enable service:" << enableReply.error().message();
        return false;
    }

    QDBusReply<void> reloadReply = systemd.call("Reload");
    if (!reloadReply.isValid()) {
        qCWarning(sm) << "Failed to reload systemd:" << reloadReply.error().message();
    }

    QDBusReply<QDBusObjectPath> startReply = systemd.call(
        "StartUnit",
        QString(PROJECT_NAME) + ".service",
        "replace"
    );

    if (!startReply.isValid()) {
        qCWarning(sm) << "Failed to start service:" << startReply.error().message();
    }

    return true;
}

bool SystemdManager::disableServiceViaDBus() {
    QDBusInterface systemd("org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          QDBusConnection::sessionBus());

    if (!systemd.isValid()) {
        qCWarning(sm) << "Cannot connect to systemd via D-Bus";
        return false;
    }

QDBusMessage stopReply = systemd.call(
        "StopUnit",
        QString(PROJECT_NAME) + ".service",
        "replace"
    );

    if (stopReply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(sm) << "Failed to stop service:" << stopReply.errorMessage();
    }

    // Bruk QDBusMessage her siden DisableUnitFiles returnerer a(sss), ikke bool
    QDBusMessage disableReply = systemd.call(
        "DisableUnitFiles",
        QStringList() << QString(PROJECT_NAME) + ".service",
        false // runtime
    );

    if (disableReply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(sm) << "Failed to disable service:" << disableReply.errorMessage();
        return false;
    }

    QDBusMessage reloadReply = systemd.call("Reload");
    if (reloadReply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(sm) << "Failed to reload systemd:" << reloadReply.errorMessage();
    }

    return true;
}

// Sett opp autostart, beholder original logikk
bool SystemdManager::setupAutostart(bool enabled) {
    qCDebug(sm) << "Setting autostart to:" << enabled;

    if (enabled) {
        qCDebug(sm) << "Writing service file to:" << serviceFilePath();
        if (!writeServiceFile()) {
            qCWarning(sm) << "Failed to write service file";
            return false;
        }
        qCDebug(sm) << "Enabling service via D-Bus";
        return enableServiceViaDBus();
    } else {
        qCDebug(sm) << "Disabling service via D-Bus";
        bool success = disableServiceViaDBus();
        if (!success) {
            qCWarning(sm) << "Failed to disable service via D-Bus";
        }

        qCDebug(sm) << "Removing service file";
        if (!removeServiceFile()) {
            qCDebug(sm) << "Service file already removed or couldn't be removed";
        }
        return success;
    }
}

// Restaurert funksjon med ekstra check av ExecStart
bool SystemdManager::isAutostartEnabled()
{
    // 1. Sjekk via D-Bus om enheten finnes og er enabled
    QDBusInterface systemd("org.freedesktop.systemd1",
                           "/org/freedesktop/systemd1",
                           "org.freedesktop.systemd1.Manager",
                           QDBusConnection::sessionBus());
    if (!systemd.isValid()) {
        qCWarning(sm) << "Cannot connect to systemd via D-Bus";
        return false;
    }

    QDBusReply<QString> stateReply = systemd.call("GetUnitFileState", QString(PROJECT_NAME) + ".service");
    if (!stateReply.isValid()) {
        qCDebug(sm) << "Failed to get service state:" << stateReply.error().message();
        return false;
    }

    QString state = stateReply.value();
    bool enabled = state == "enabled" || state == "enabled-runtime" || state == "static";
    if (!enabled) {
        qCDebug(sm) << "Service is not enabled in systemd";
        return false;
    }

    // 2. Sjekk at service-filen eksisterer
    QFile file(serviceFilePath());
    if (!file.exists()) {
        qCDebug(sm) << "Service file missing:" << serviceFilePath();
        return false;
    }

    // 3. Sjekk at ExecStart matcher plattformen
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(file.readAll());
        file.close();

        QString expectedExec = "ExecStart=" + QCoreApplication::applicationFilePath();

        if (!content.contains(expectedExec)) {
            qCDebug(sm) << "Service ExecStart mismatch for current platform";
            return false;
        }
    } else {
        qCDebug(sm) << "Failed to open service file for reading:" << file.errorString();
        return false;
    }

    // Alt OK
    return true;
}
