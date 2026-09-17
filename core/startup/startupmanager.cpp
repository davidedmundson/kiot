#include "Shared/platformhelper.h"
#include "startupmanager.h"
#include "systemdmanager.h" 
#include "backgroundmanager.h"
#include "desktopmanager.h"

#include <QObject>
#include <QCoreApplication>

DEFINE_LOGGER(sum, Core.Startup.StartupManager)

StartupManager::StartupManager(QObject *parent)
    : QObject(parent),
      m_systemdManager(new SystemdManager(this)),
      m_desktopManager(new DesktopManager(this)),
      m_backgroundManager(new BackgroundManager(this)) {}

StartupManager::~StartupManager() = default;

bool StartupManager::isAutostartEnabled() {
    if (PlatformHelper::isFlatpak() && m_backgroundManager->isAvailable()) {
        return m_backgroundManager->isAutostartEnabled();
    } else if (m_systemdManager->isAvailable()) {
        return m_systemdManager->isAutostartEnabled();
    } else if (m_desktopManager->isAvailable()) {
        return m_desktopManager->isAutostartEnabled();
    }
    
    return false;
}

bool StartupManager::setAutostart(bool enabled) {
    // 1. Prioriter Flatpak Background Portal hvis vi er i Flatpak og den er tilgjengelig
    
    if (PlatformHelper::isFlatpak() && m_backgroundManager->isAvailable()) {
        qCDebug(sum) << "Running in Flatpak, delegating autostart configuration to BackgroundManager";
        if (m_backgroundManager->setupAutostart(enabled)) {
            return true;
        }
        qCWarning(sum) << "BackgroundManager autostart failed, attempting fallback";
    }

    // 2. Prøv Systemd hvis tilgjengelig (og ikke i Flatpak)
    if (!PlatformHelper::isFlatpak() && m_systemdManager->isAvailable()) {
        qCDebug(sum) << "Delegating autostart configuration to SystemdManager";
        if (m_systemdManager->setupAutostart(enabled)) {
            return true;
        }
        qCWarning(sum) << "Systemd autostart failed, attempting fallback to .desktop";
    }

    // 3. Fallback til DesktopManager (.desktop-filer)
    if (m_desktopManager->isAvailable()) {
        qCDebug(sum) << "Using DesktopManager for autostart";
        bool success = m_desktopManager->setupAutostart(enabled);
        if (success) {
            return true;
        }
    }

    // Hvis absolutt alt feiler og vi prøver å enable
    if (enabled) {
        qCCritical(sum) << "CRITICAL: Autostart could not be enabled in this environment!";
        qCCritical(sum) << "Step-by-step for manual configuration:";
        qCCritical(sum) << "1. Open your desktop session's autostart settings.";
        qCCritical(sum) << "2. Add a new application entry manually pointing to: " << QCoreApplication::applicationFilePath();
    }

    return false;
}