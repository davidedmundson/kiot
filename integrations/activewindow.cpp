// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/sensor.h"
#include <KProcess>
#include <KSandbox>

#include <QApplication>
#include <QGuiApplication>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcessEnvironment>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(aw)
Q_LOGGING_CATEGORY(aw, "integration.ActiveWindow")

class KDEActiveWindowWatcher : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.davidedmundson.kiot.ActiveWindow")

public:
    explicit KDEActiveWindowWatcher(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_sensor = new Sensor(this);
        m_sensor->setId("active_window");
        m_sensor->setName("Active Window");
        m_sensor->setDiscoveryConfig("icon", "mdi:application");
        // Register DBus service first
        if (!QDBusConnection::sessionBus().registerService("org.davidedmundson.kiot.ActiveWindow")) {
            qCWarning(aw) << "ActiveWindowWatcher: Failed to register DBus service";
            m_sensor->setState("Unavailable");
            return;
        }

        if (!QDBusConnection::sessionBus().registerObject("/ActiveWindow", "org.davidedmundson.kiot.ActiveWindow", this, QDBusConnection::ExportAllSlots)) {
            qCWarning(aw) << "ActiveWindowWatcher: Failed to register DBus object";
            m_sensor->setState("Unavailable");
            return;
        }

        if (!registerKWinScript()) {
            qCWarning(aw) << "ActiveWindowWatcher: Failed to register KWin script";
            m_sensor->setState("Unavailable");
            return;
        }
    }

    ~KDEActiveWindowWatcher()
    {
        cleanup();
    }

public slots:
    Q_SCRIPTABLE void UpdateAttributes(const QVariantMap &attributes)
    {
        QString title = attributes["title"].toString();
        if (title != m_sensor->state()) {
            m_sensor->setState(title);
        }
        m_sensor->setAttributes(attributes);
    }

private:
    bool registerKWinScript()
    {
        m_kwinIface = new QDBusInterface("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", QDBusConnection::sessionBus(), this);

        if (!m_kwinIface->isValid()) {
            qCWarning(aw) << "ActiveWindowWatcher: KWin scripting interface not available";
            return false;
        }

        // Clean up any existing instance
        cleanup();

        // Locate installed KWin script from KDE data dirs
        m_scriptPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kiot/activewindow_kwin.js"));
        if (m_scriptPath.isEmpty()) {
            qCWarning(aw) << "ActiveWindowWatcher: installed KWin script not found in data dirs";
            return false;
        }
        if (KSandbox::isFlatpak()) {
            // Create needed kiot path
            if (!QDir("/var/cache/kiot/").exists())
                QDir().mkdir("/var/cache/kiot/");

            // Remove script just incase we have a new updated one
            if (!QFile("/var/cache/kiot/activewindow_kwin.js").exists())
                QFile::remove("/var/cache/kiot/activewindow_kwin.js");
            // Copy script to location available outside flatpak sandbox
            QFile::copy(m_scriptPath, "/var/cache/kiot/activewindow_kwin.js");

            m_scriptPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/activewindow_kwin.js";
        }
        QDBusMessage reply = m_kwinIface->call("loadScript", m_scriptPath, "kiot_activewindow");
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qCWarning(aw) << "ActiveWindowWatcher: loadScript failed:" << reply.errorMessage();
            return false;
        }

        if (reply.arguments().isEmpty()) {
            qCWarning(aw) << "ActiveWindowWatcher: loadScript returned no arguments";
            return false;
        }

        QVariant arg = reply.arguments().first();
        QString scriptObjectPath;

        // KWin returns an int for the script ID in all supported versions
        if (arg.canConvert<int>()) {
            int id = arg.toInt();
            scriptObjectPath = QString("/Scripting/Script%1").arg(id);
        } else {
            qCWarning(aw) << "ActiveWindowWatcher: Unexpected return type from loadScript:" << arg.typeName();
            return false;
        }
        QDBusInterface scriptIface("org.kde.KWin", scriptObjectPath, "org.kde.kwin.Script", QDBusConnection::sessionBus());
        if (!scriptIface.isValid()) {
            qCWarning(aw) << "ActiveWindowWatcher: scriptIface invalid for path" << scriptObjectPath;
            QFile::remove(m_scriptPath);
            return false;
        }

        QDBusMessage runReply = scriptIface.call("run");
        if (runReply.type() == QDBusMessage::ErrorMessage) {
            qCWarning(aw) << "ActiveWindowWatcher: run failed:" << runReply.errorMessage();
            QFile::remove(m_scriptPath);
            return false;
        }

        return true;
    }

    void cleanup()
    {
        if (m_kwinIface) {
            m_kwinIface->call("unloadScript", "kiot_activewindow");
        }
    }

    Sensor *m_sensor;
    QDBusInterface *m_kwinIface = nullptr;
    QString m_scriptPath;
    bool m_connected = false;
};

/**
 * @brief A helper function to detect Desktop Enviornment
 */
static QString detectDesktopEnvironment()
{
    // 1. Check XDG_CURRENT_DESKTOP first
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    if (!desktop.isEmpty()) {
        QString lowerDesktop = desktop.toLower();

        // Moderne KDE Plasma setter vanligvis "KDE" eller "Plasma"
        if (lowerDesktop.contains("kde") || lowerDesktop.contains("plasma")) {
            return "kde";
        }

        // GNOME og derivater
        if (lowerDesktop.contains("gnome") || lowerDesktop.contains("ubuntu") || lowerDesktop.contains("pop") || lowerDesktop.contains("cosmic")) {
            return "gnome";
        }

        return lowerDesktop;
    }

    // 2. Check DESKTOP_SESSION (For older systems)
    desktop = qEnvironmentVariable("DESKTOP_SESSION");
    if (!desktop.isEmpty()) {
        QString lowerDesktop = desktop.toLower();
        if (lowerDesktop.contains("plasma") || lowerDesktop.contains("kde")) {
            return "kde";
        }
        if (lowerDesktop.contains("gnome") || lowerDesktop.contains("ubuntu")) {
            return "gnome";
        }
        return lowerDesktop;
    }

    // 3. Check plasma specific process
    QString program = "pgrep";
    QStringList arguments = QStringList() << "-x" << "plasmashell";
    KProcess kprocess;
    kprocess.setProgram(program);
    kprocess.setArguments(arguments);
    if (KSandbox::isFlatpak()) {
        KSandbox::ProcessContext ctx = KSandbox::makeHostContext(kprocess);
        kprocess.setProgram(ctx.program);
        kprocess.setArguments(ctx.arguments);
    }
    kprocess.start();
    kprocess.waitForFinished();
    if (kprocess.exitCode() == 0) {
        return "kde";
    }

    // 4. Check Gnome specific process
    arguments = QStringList() << "-x" << "gnome-shell";
    kprocess.setProgram(program);
    kprocess.setArguments(arguments);
    if (KSandbox::isFlatpak()) {
        KSandbox::ProcessContext ctx = KSandbox::makeHostContext(kprocess);
        kprocess.setProgram(ctx.program);
        kprocess.setArguments(ctx.arguments);
    }
    kprocess.start();
    kprocess.waitForFinished();
    if (kprocess.exitCode() == 0) {
        return "gnome";
    }

    // 5. Sjekk XDG_SESSION_DESKTOP (nyere standard)
    desktop = qEnvironmentVariable("XDG_SESSION_DESKTOP");
    if (!desktop.isEmpty()) {
        QString lowerDesktop = desktop.toLower();
        if (lowerDesktop.contains("kde") || lowerDesktop.contains("plasma")) {
            return "kde";
        }
        if (lowerDesktop.contains("gnome")) {
            return "gnome";
        }
        return lowerDesktop;
    }

    return "unknown";
}

void setupActiveWindow()
{
    QString desktop = detectDesktopEnvironment();
    qCInfo(aw) << "Detected desktop environment:" << desktop;

    if (desktop.contains("kde") || desktop.contains("plasma")) {
        qCInfo(aw) << "Initializing KDE ActiveWindowWatcher";
        new KDEActiveWindowWatcher(qApp);
    } 
    else {
        qCWarning(aw) << "Unsupported desktop environment:" << desktop;
        // Create a sensor anyway but mark it as unavailable
        Sensor *sensor = new Sensor(qApp);
        sensor->setId("active_window");
        sensor->setName("Active Window");
        sensor->setDiscoveryConfig("icon", "mdi:application");
        sensor->setState("Unavailable - Unsupported DE: " + desktop);
    }
}

REGISTER_INTEGRATION("ActiveWindow", setupActiveWindow, true)
#include "activewindow.moc"
