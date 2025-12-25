#include "servicemanager.h"
#include <QObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <KSandbox>
#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(sm)
Q_LOGGING_CATEGORY(sm, "kiot.ServiceManager")

ServiceManager::ServiceManager(QObject *parent) : QObject(parent) {}


QString ServiceManager::serviceFilePath() {
    if(KSandbox::isFlatpak())
        return QDir::homePath() + "/.config/systemd/user/kiot.service";
    
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return configDir + "/systemd/user/kiot.service";
}

QString ServiceManager::serviceContent() {
    QString execLine;
    
    if (KSandbox::isFlatpak()) {
        execLine = "ExecStart=/usr/bin/flatpak run --branch=master --arch=x86_64 --command=kiot org.davidedmundson.kiot";
    } else {
        execLine = "ExecStart=/usr/bin/kiot";
    }
    
    return QStringLiteral(
        "[Unit]\n"
        "Description=Kiot - KDE IOT Connection\n"
        "Documentation=https://github.com/davidedmundson/kiot\n"
        "Wants=network-online.target\n"
        "After=network-online.target graphical-session.target\n"
        "\n"
        "[Service]\n"
        "Type=simple\n"
        "%1\n"
        "Restart=on-failure\n"
        "RestartSec=3\n"
        "\n"
        "Slice=user.slice\n"
        "\n"
        "[Install]\n"
        "WantedBy=default.target\n"
    ).arg(execLine);
}

bool ServiceManager::writeServiceFile() {
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

bool ServiceManager::removeServiceFile() {
    QFile file(serviceFilePath());
    return file.remove();
}

bool ServiceManager::enableServiceViaDBus() {
    QDBusInterface systemd("org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          QDBusConnection::sessionBus());
    
    if (!systemd.isValid()) {
        qCWarning(sm) << "Cannot connect to systemd via D-Bus";
        return false;
    }
    

    QDBusReply<bool> enableReply = systemd.call(
        "EnableUnitFiles",
        QStringList() << "kiot.service",
        false, 
        true   
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
        "kiot.service",
        "replace"
    );
    
    if (!startReply.isValid()) {
        qCWarning(sm) << "Failed to start service:" << startReply.error().message();

    }
    
    return true;
}

bool ServiceManager::disableServiceViaDBus() {
    QDBusInterface systemd("org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          QDBusConnection::sessionBus());
    
    if (!systemd.isValid()) {
        qCWarning(sm) << "Cannot connect to systemd via D-Bus";
        return false;
    }
    

    QDBusReply<bool> disableReply = systemd.call(
        "DisableUnitFiles",
        QStringList() << "kiot.service",
        false

    );
    
    if (!disableReply.isValid()) {
        qCWarning(sm) << "Failed to disable service:" << disableReply.error().message();
        return false;
    }
    
    qCDebug(sm) << "Service disabled successfully";
    

    QDBusReply<void> reloadReply = systemd.call("Reload");
    if (!reloadReply.isValid()) {
        qCWarning(sm) << "Failed to reload systemd:" << reloadReply.error().message();
    }
    
    return true;
}

bool ServiceManager::setupAutostart(bool enabled) {
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

bool ServiceManager::isAutostartEnabled() {
    QDBusInterface systemd("org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          QDBusConnection::sessionBus());
    
    if (!systemd.isValid()) {
        qCWarning(sm) << "Cannot connect to systemd via D-Bus";
        return false;
    }
    
    QDBusReply<QString> reply = systemd.call("GetUnitFileState", "kiot.service");
    
    if (reply.isValid()) {
        QString state = reply.value();
        qCDebug(sm) << "Service state:" << state;
        return state == "enabled" || state == "enabled-runtime" || state == "static";
    } else {
        qCWarning(sm) << "Failed to get service state:" << reply.error().message();
        return false;
    }
}

