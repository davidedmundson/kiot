#include "Shared/platformhelper.h"
#include "backgroundmanager.h"
#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QLoggingCategory>
#include <KSharedConfig>
#include <KConfigGroup>

DEFINE_LOGGER(bgm, Core.Startup.BackgroundManager)


BackgroundManager::BackgroundManager(QObject *parent)
    : QObject(parent)
    , m_backgroundIface(new OrgFreedesktopPortalBackgroundInterface(QStringLiteral("org.freedesktop.portal.Desktop"), QStringLiteral("/org/freedesktop/portal/desktop"),QDBusConnection::sessionBus(),this))
{
}

bool BackgroundManager::setupAutostart(bool enabled)
{

    if (enabled) {
        if(enableAutostartup())
        {
            KSharedConfigPtr config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
            KConfigGroup startupGroup(config, "general");
            startupGroup.writeEntry("BackgroundAutostartEnabled", enabled);
            config->sync(); 
            return true;
        }
        return false;
    } else {
        if(disableAutostartup())
        {
            KSharedConfigPtr config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
            KConfigGroup startupGroup(config, "general");
            startupGroup.writeEntry("BackgroundAutostartEnabled", enabled);
            config->sync(); 
            return true;
        }
        return false;
    }


}
bool BackgroundManager::isAvailable()
{
    return m_backgroundIface->isValid();
}
bool BackgroundManager::isAutostartEnabled() const
{
    //WORKAROUND temprorary until we can find this dynamically
    KSharedConfigPtr config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    KConfigGroup startupGroup(config, "general");
    
    return startupGroup.readEntry("BackgroundAutostartEnabled", false);
}

bool BackgroundManager::enableAutostartup()
{
    if (!m_backgroundIface->isValid()) {
        qCWarning(bgm) << "Background portal interface is not valid on session bus!";
        return false;
    }
    QString name = PlatformHelper::getProjectName();
    QVariantMap options;
    options.insert(QStringLiteral("autostart"), true);
    options.insert(QStringLiteral("reason"), name.toUpper() + QStringLiteral(" needs to run in the background to enable automatic startup and handle smart home automations, MQTT listeners, and system monitoring."));
    options.insert(QStringLiteral("silence"), false);

    QDBusPendingReply<QDBusObjectPath> reply = m_backgroundIface->RequestBackground(QString(), options);
    reply.waitForFinished();

    if (reply.isError()) {
        qCWarning(bgm) << "Failed to request background execution:" << reply.error().message();
        return false;
    }

    
    qCDebug(bgm) << "Background request successful! Handle path:" << reply.value().path();
    return true;
}

bool BackgroundManager::disableAutostartup()
{
    if (!m_backgroundIface->isValid()) {
        qCWarning(bgm) << "Background portal interface is not valid on session bus!";
        return false;
    }
    QString name = PlatformHelper::getProjectName();
   
    QVariantMap options;
    options.insert(QStringLiteral("autostart"), false);
    options.insert(QStringLiteral("reason"), name.toUpper() + QStringLiteral(" needs to run in the background to disable automatic startup and handle smart home automations, MQTT listeners, and system monitoring."));
    options.insert(QStringLiteral("silence"), false);
    
    QDBusPendingReply<QDBusObjectPath> reply = m_backgroundIface->RequestBackground(QString(), options);
    reply.waitForFinished();

    if (reply.isError()) {
        qCWarning(bgm) << "Failed to disable background execution:" << reply.error().message();
        return false;
    }

    qCDebug(bgm) << "Background autostart disabled successfully.";
    return true;
}