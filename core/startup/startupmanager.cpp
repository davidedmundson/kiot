
#include "Shared/platformhelper.h"
#include "startupmanager.h"
#include "systemdmanager.h" 
#include "backgroundmanager.h"
#include "desktopmanager.h"

#include <QObject>
#include <QGuiApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

DEFINE_LOGGER(sum, Core.Startup.StartupManager)

StartupManager::StartupManager(QObject *parent)
    : QObject(parent),
      m_systemdManager(new SystemdManager(this)),
      m_desktopManager(new DesktopManager(this)),
      m_backgroundManager(new BackgroundManager(this)) {}

StartupManager::~StartupManager() = default;


bool StartupManager::shouldUseSystemd() const {
    // Flatpak check
    if (PlatformHelper::isFlatpak()) {
        qCDebug(sum) << "Running in Flatpak, preferring backgroundmanager for autostart";
        return false;
    }
    //Check that systemd is available for uswe
    QDBusInterface systemd("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",QDBusConnection::sessionBus());

    if (!systemd.isValid()) {
        qCWarning(sum) << "Systemd session bus is not available or not running.";
        return false;
    }

    // Final validation og systemd availability
    QDBusReply<void> reply = systemd.call("Dump");
    if (!reply.isValid()) {
        qCWarning(sum) << "Systemd D-Bus is responding, but method call failed:" << reply.error().message();
        return false;
    }

    // Lets use systemd =D
    return true; 
}

bool StartupManager::isAutostartEnabled() {
    if (shouldUseSystemd()) {
        return m_systemdManager->isAutostartEnabled();
    } else if (PlatformHelper::isFlatpak()) {
        return m_backgroundManager->isAutostartEnabled();
    } else {
        return m_desktopManager->isAutostartEnabled();
    }
    return false;
}

bool StartupManager::setAutostart(bool enabled) {
    if (shouldUseSystemd()) {
        qCDebug(sum) << "Delegating autostart configuration to SystemdManager";
        if (m_systemdManager->setupAutostart(enabled)) {
            return true;
        }
        qCWarning(sum) << "Systemd autostart failed, attempting fallback to .desktop";
    }
    else if (PlatformHelper::isFlatpak()) {
        qCDebug(sum) << "Delegating autostart configuration to BackgroundManager";
        if (m_backgroundManager->setupAutostart(enabled))
        {
            return  true;
        }
        qCWarning(sum) << "BackgroundManager autostart failed, attempting fallback to .desktop";
    }
    // Fallback 
    qCDebug(sum) << "Using DesktopManager for autostart";
    bool success = m_desktopManager->setupAutostart(enabled);

    if (!success && enabled) {
        qCCritical(sum) << "CRITICAL: Autostart could not be enabled in this environment!";
        qCCritical(sum) << "Step-by-step for manual configuration:";
        qCCritical(sum) << "1. Open your desktop session's autostart settings.";
        qCCritical(sum) << "2. Add a new application entry manually pointing to: " << QCoreApplication::applicationFilePath();
    }

    return success;
}