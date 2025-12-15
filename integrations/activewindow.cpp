// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/entities.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>
#include <QTimer>
#include <QDir>
class ActiveWindowWatcher : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.davidedmundson.kiot.ActiveWindow")

public:
    explicit ActiveWindowWatcher(QObject *parent = nullptr);
    ~ActiveWindowWatcher();
public slots:
    Q_SCRIPTABLE void UpdateAttributes(const QVariantMap &attributes);

private:
    bool registerKWinScript();
    void cleanup();
    Sensor *m_sensor;
    QDBusInterface *m_kwinIface = nullptr;
    QString m_lastTitle;
    QString m_scriptPath;
    bool m_connected = false;
};

ActiveWindowWatcher::ActiveWindowWatcher(QObject *parent)
    : QObject(parent)
{
    m_sensor = new Sensor(this);
    m_sensor->setId("active_window");
    m_sensor->setName("Active Window");
    m_sensor->setDiscoveryConfig("icon", "mdi:application");
    // Register DBus service first
    if (!QDBusConnection::sessionBus().registerService("org.davidedmundson.kiot.ActiveWindow")) {
        qWarning() << "ActiveWindowWatcher: Failed to register DBus service";
        m_sensor->setState("Unavailable");
        return;
    }

    if (!QDBusConnection::sessionBus().registerObject("/ActiveWindow", "org.davidedmundson.kiot.ActiveWindow", this, QDBusConnection::ExportAllSlots)) {
        qWarning() << "ActiveWindowWatcher: Failed to register DBus object";
        m_sensor->setState("Unavailable");
        return;
    }

    if (!registerKWinScript()) {
        qWarning() << "ActiveWindowWatcher: Failed to register KWin script";
        m_sensor->setState("Unavailable");
        return;
    }
}

ActiveWindowWatcher::~ActiveWindowWatcher()
{
    cleanup();
}

void ActiveWindowWatcher::cleanup()
{
    m_kwinIface->call("unloadScript", "kiot_activewindow");
}
bool ActiveWindowWatcher::registerKWinScript()
{
    m_kwinIface = new QDBusInterface("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", QDBusConnection::sessionBus(), this);

    if (!m_kwinIface->isValid()) {
        qWarning() << "ActiveWindowWatcher: KWin scripting interface not available";
        return false;
    }

    // Clean up any existing instance
    cleanup();

    // Locate installed KWin script from KDE data dirs
    m_scriptPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kiot/activewindow_kwin.js"));
    if (m_scriptPath.isEmpty()) {
        qWarning() << "ActiveWindowWatcher: installed KWin script not found in data dirs";
        return false;
    }
    if (m_scriptPath.startsWith(QLatin1String("/app/")))
    {
        QFile::copy(m_scriptPath, "/var/config/activewindow_kwin.js");
        m_scriptPath =    QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/activewindow_kwin.js";
    }
    QDBusMessage reply = m_kwinIface->call("loadScript", m_scriptPath, "kiot_activewindow");
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "ActiveWindowWatcher: loadScript failed:" << reply.errorMessage();
        return false;
    }

    if (reply.arguments().isEmpty()) {
        qWarning() << "ActiveWindowWatcher: loadScript returned no arguments";
        return false;
    }

    QVariant arg = reply.arguments().first();
    QString scriptObjectPath;

    // KWin returns an int for the script ID in all supported versions
    if (arg.canConvert<int>()) {
        int id = arg.toInt();
        scriptObjectPath = QString("/Scripting/Script%1").arg(id);
    } else {
        qWarning() << "ActiveWindowWatcher: Unexpected return type from loadScript:" << arg.typeName();
        return false;
    }
    QDBusInterface scriptIface("org.kde.KWin", scriptObjectPath, "org.kde.kwin.Script", QDBusConnection::sessionBus());
    if (!scriptIface.isValid()) {
        qWarning() << "ActiveWindowWatcher: scriptIface invalid for path" << scriptObjectPath;
        return false;
    }

    QDBusMessage runReply = scriptIface.call("run");
    if (runReply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "ActiveWindowWatcher: run failed:" << runReply.errorMessage();
        return false;
    }

    return true;
}

void ActiveWindowWatcher::UpdateAttributes(const QVariantMap &attributes)
{
    QString title = attributes["title"].toString();
    if (title != m_lastTitle) {
        m_lastTitle = title;
        m_sensor->setState(title);
    }
    m_sensor->setAttributes(attributes);
}

void setupActiveWindow()
{
    new ActiveWindowWatcher(qApp);
}

REGISTER_INTEGRATION("ActiveWindow", setupActiveWindow, true)
#include "activewindow.moc"
