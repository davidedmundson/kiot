
#include "Shared/platformhelper.h"
#include "startupmanager.h"
#include "systemdmanager.h" 
#include "desktopmanager.h"
#include <QObject>
#include <QGuiApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

DEFINE_LOGGER(sum, core.StartupManager)

StartupManager::StartupManager(QObject *parent)
    : QObject(parent),
      m_systemdManager(new SystemdManager(this)),
      m_desktopManager(new DesktopManager(this)) {}

StartupManager::~StartupManager() = default;

// Sjekker om vi har et aktivt systemd-miljø tilgjengelig
bool StartupManager::shouldUseSystemd() const {
    // Flatpak check
    if (PlatformHelper::isFlatpak()) {
        qCDebug(sum) << "Running in Flatpak, preferring .desktop autostart";
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
    } else {
        return m_desktopManager->isAutostartEnabled();
    //TODO implement desktopmanager and return the
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

    // Fallback eller direkte valg for .desktop
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