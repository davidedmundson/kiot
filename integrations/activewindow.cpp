// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core/core.h"
#include "Shared/entities/sensor.h"
#include "Shared/platformhelper.h"
#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>


DEFINE_LOGGER(aw, Integrations.ActiveWindow)

class KDEActiveWindowWatcher : public QObject
{
    Q_OBJECT

public:
    explicit KDEActiveWindowWatcher(QObject *parent = nullptr)
        : QObject(parent)
    {
        readScriptResource();

        m_sensor = new Sensor(this);
        m_sensor->setId("active_window");
        m_sensor->setName("Active Window");
        m_sensor->setDiscoveryConfig("icon", "mdi:application");
        //Made this wait 2000ms because dbus is slower than kiot on startup and it will fail to register multiple times with notifications
        QTimer::singleShot(2000, this, &KDEActiveWindowWatcher::tryRegisterDBus);
    }

    ~KDEActiveWindowWatcher()
    {
        cleanup();
        if (!m_scriptPath.isEmpty() && QFile::exists(m_scriptPath)) {
            QFile::remove(m_scriptPath);
        }
        if (PlatformHelper::isFlatpak()) {
            auto new_path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            QFile f(new_path + "/activewindow_kwin.js");
            if (f.exists()) f.remove();
        }
    }
public slots:
    Q_SCRIPTABLE void UpdateAttributes(const QVariantMap &attributes)
    {
        //qCDebug(aw) << "UpdateAttributes called with:" << attributes;
        QString title = attributes["title"].toString();
        if (title != m_sensor->state()) {
            m_sensor->setState(title);
        }
        m_sensor->setAttributes(attributes);
    }

private:
    void readScriptResource()
    {
        QString kwinScriptPath = ":/kwin/activewindow_kwin.js";
        QFile file(kwinScriptPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(aw) << "Failed to open KWin script resource";
            return;
        }

        QString scriptContent = QString::fromUtf8(file.readAll());
        file.close();
        //TODO add a servicename replacer for a just working script even in rebranded forks
        scriptContent.replace("org.davidedmundson.kiot", PlatformHelper::generateServiceName());

        QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/activewindow_kwin.js";
        if (QFile::exists(tempPath)) QFile::remove(tempPath);

        QFile outFile(tempPath);
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            outFile.write(scriptContent.toUtf8());
            outFile.close();
            m_scriptPath = tempPath;
        } else {
            qCWarning(aw) << "Failed to write KWin script to temporary file";
        }
    }

    void tryRegisterDBus()
    {
        if (m_dbusRetries >= MAX_RETRIES) {
            qCWarning(aw) << "ActiveWindowWatcher: DBus registration failed after max retries";
            m_sensor->setState("Unavailable - DBus failed");
            return;
        }
        //TODO implement dynamic servicename generation
        const QString serviceName = PlatformHelper::generateServiceName() + ".ActiveWindow";
      //  const QString serviceName = QString(APP_ID) + ".ActiveWindow";
            
        if (QDBusConnection::sessionBus().registerService(serviceName) &&
            QDBusConnection::sessionBus().registerObject("/ActiveWindow", serviceName, this, QDBusConnection::ExportAllSlots))
        {
            qCInfo(aw) << "ActiveWindowWatcher: DBus ready";
            QTimer::singleShot(0, this, &KDEActiveWindowWatcher::tryInitKWin);
            return;
        }

        m_dbusRetries++;
        int interval = 500 * (1 << (m_dbusRetries - 1));
        interval = qMin(interval, 8000); // maks 8s
        qCWarning(aw) << "ActiveWindowWatcher: DBus not ready, retrying in " << interval << "ms";
        QTimer::singleShot(interval, this, &KDEActiveWindowWatcher::tryRegisterDBus);
    }

    void tryInitKWin()
    {
        if (m_kwinRetries >= MAX_RETRIES) {
            qCWarning(aw) << "ActiveWindowWatcher: KWin script failed after max retries";
            m_sensor->setState("Unavailable - KWin script failed");
            return;
        }

        if (registerKWinScript()) {
            qCInfo(aw) << "ActiveWindowWatcher: KWin ready";
            return;
        }

        m_kwinRetries++;
        int interval = 500 * (1 << (m_kwinRetries - 1));
        interval = qMin(interval, 8000); // maks 8s
        qCInfo(aw) << "ActiveWindowWatcher: KWin not ready, retrying in" << interval << "ms";
        QTimer::singleShot(interval, this, &KDEActiveWindowWatcher::tryInitKWin);
    }

    bool registerKWinScript()
    {
        if (!m_kwinIface) {
            m_kwinIface = new QDBusInterface("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", QDBusConnection::sessionBus(), this);
        }

        if (!m_kwinIface->isValid()) {
            return false;
        }

        cleanup();

        QString scriptPathToUse = m_scriptPath;
        if (PlatformHelper::isFlatpak()) {
            auto new_path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            if (!QDir(new_path).exists()) QDir().mkpath(new_path);

            QFile f(new_path + "/activewindow_kwin.js");
            if (f.exists()) f.remove();
            QFile::copy(m_scriptPath, new_path + "/activewindow_kwin.js");
            scriptPathToUse = new_path + "/activewindow_kwin.js";
        }

        QDBusMessage reply = m_kwinIface->call("loadScript", scriptPathToUse, "kiot_activewindow");
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            qCWarning(aw) << "Failed to load script" << reply.errorMessage();
            return false;
        }

        QVariant arg = reply.arguments().first();
        int scriptId = arg.toInt();
        QString scriptObjectPath = QString("/Scripting/Script%1").arg(scriptId);
        //qCDebug(aw) << "Script loaded at" << scriptObjectPath;
        QDBusInterface scriptIface("org.kde.KWin", scriptObjectPath, "org.kde.kwin.Script", QDBusConnection::sessionBus());
        if (!scriptIface.isValid()) return false;

        QDBusMessage runReply = scriptIface.call("run");
        if (runReply.type() == QDBusMessage::ErrorMessage) {
            //qCWarning(aw) << "Failed to run script" << runReply.errorMessage();
            return false;
        }

        return true;
    }

    void cleanup()
    {
        if (m_kwinIface && m_kwinIface->isValid()) {
            m_kwinIface->call("unloadScript", "kiot_activewindow");
        }
    }

private:
    static constexpr int MAX_RETRIES = 5;

    Sensor *m_sensor = nullptr;
    QDBusInterface *m_kwinIface = nullptr;
    QString m_scriptPath;
    int m_dbusRetries = 0;
    int m_kwinRetries = 0;
};

void setupActiveWindow()
{
    if(PlatformHelper::detectDesktopEnvironment() != "kde")
    {
        qCDebug(aw) << "KDE Active Window integration is only supported on KDE Plasma";
        qCDebug(aw) << "Disable it in you config file under [integrations] and set ActiveWindow=false";
        return;
    }
    new KDEActiveWindowWatcher(qApp);
}

REGISTER_INTEGRATION("ActiveWindow", setupActiveWindow, true)

#include "activewindow.moc"
