// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "kdeactivewindowwatcher.h"

#include <KIOTShared/kiotshared.h>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>
#include <QLoggingCategory>

using KIOTShared::Entities::Sensor;
using KIOTShared::PlatformHelper;

Q_DECLARE_LOGGING_CATEGORY(plugin_logger_activewindow)

KDEActiveWindowWatcher::KDEActiveWindowWatcher(QObject *parent)
    : QObject(parent)
{
    readScriptResource();

    m_sensor = new Sensor(this);
    m_sensor->setId("active_window");
    m_sensor->setName("Active Window");
    m_sensor->setDiscoveryConfig("icon", "mdi:application");
    
    QTimer::singleShot(2000, this, &KDEActiveWindowWatcher::tryRegisterDBus);
}

KDEActiveWindowWatcher::~KDEActiveWindowWatcher()
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

void KDEActiveWindowWatcher::UpdateAttributes(const QVariantMap &attributes)
{
    QString title = attributes["title"].toString();
    if (title != m_sensor->state()) {
        m_sensor->setState(title);
    }
    m_sensor->setAttributes(attributes);
}

void KDEActiveWindowWatcher::readScriptResource()
{
    QString kwinScriptPath = ":/kwin/activewindow_kwin.js";
    QFile file(kwinScriptPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(plugin_logger_activewindow) << "Failed to open KWin script resource";
        return;
    }

    QString scriptContent = QString::fromUtf8(file.readAll());
    file.close();
    scriptContent.replace("org.davidedmundson.kiot", PlatformHelper::generateServiceName());

    // private to the user (0700), unlike /tmp
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/activewindow_kwin.js";
    if (QFile::exists(tempPath)) QFile::remove(tempPath);

    QFile outFile(tempPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        outFile.write(scriptContent.toUtf8());
        outFile.close();
        m_scriptPath = tempPath;
    } else {
        qCWarning(plugin_logger_activewindow) << "Failed to write KWin script to temporary file";
    }
}

void KDEActiveWindowWatcher::tryRegisterDBus()
{
    if (m_dbusRetries >= MAX_RETRIES) {
        qCWarning(plugin_logger_activewindow) << "ActiveWindowWatcher: DBus registration failed after max retries";
        m_sensor->setState("Unavailable - DBus failed");
        return;
    }

    const QString serviceName = PlatformHelper::generateServiceName() + ".ActiveWindow";
    qCInfo(plugin_logger_activewindow) << "ActiveWindowWatcher: DBus service name:" << serviceName;
        
    if (QDBusConnection::sessionBus().registerService(serviceName) &&
        QDBusConnection::sessionBus().registerObject("/ActiveWindow", serviceName, this, QDBusConnection::ExportAllSlots))
    {
        qCInfo(plugin_logger_activewindow) << "ActiveWindowWatcher: DBus ready";
        QTimer::singleShot(0, this, &KDEActiveWindowWatcher::tryInitKWin);
        return;
    }

    m_dbusRetries++;
    int interval = 500 * (1 << (m_dbusRetries - 1));
    interval = qMin(interval, 8000);
    qCWarning(plugin_logger_activewindow) << "ActiveWindowWatcher: DBus not ready, retrying in " << interval << "ms";
    QTimer::singleShot(interval, this, &KDEActiveWindowWatcher::tryRegisterDBus);
}

void KDEActiveWindowWatcher::tryInitKWin()
{
    if (m_kwinRetries >= MAX_RETRIES) {
        qCWarning(plugin_logger_activewindow) << "ActiveWindowWatcher: KWin script failed after max retries";
        m_sensor->setState("Unavailable - KWin script failed");
        return;
    }

    if (registerKWinScript()) {
        qCInfo(plugin_logger_activewindow) << "ActiveWindowWatcher: KWin ready";
        return;
    }

    m_kwinRetries++;
    int interval = 500 * (1 << (m_kwinRetries - 1));
    interval = qMin(interval, 8000);
    qCInfo(plugin_logger_activewindow) << "ActiveWindowWatcher: KWin not ready, retrying in" << interval << "ms";
    QTimer::singleShot(interval, this, &KDEActiveWindowWatcher::tryInitKWin);
}

bool KDEActiveWindowWatcher::registerKWinScript()
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
        qCWarning(plugin_logger_activewindow) << "Failed to load script" << reply.errorMessage();
        return false;
    }

    QVariant arg = reply.arguments().first();
    int scriptId = arg.toInt();
    QString scriptObjectPath =QStringLiteral("/Scripting/Script%1").arg(scriptId);
    QDBusInterface scriptIface("org.kde.KWin", scriptObjectPath, "org.kde.kwin.Script", QDBusConnection::sessionBus());
    if (!scriptIface.isValid()) return false;

    QDBusMessage runReply = scriptIface.call("run");
    if (runReply.type() == QDBusMessage::ErrorMessage) {
        return false;
    }

    return true;
}

void KDEActiveWindowWatcher::cleanup()
{
    if (m_kwinIface && m_kwinIface->isValid()) {
        m_kwinIface->call("unloadScript", "kiot_activewindow");
    }
}