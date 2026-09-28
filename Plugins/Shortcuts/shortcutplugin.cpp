
#include "shortcutplugin.h"
#include <QCoreApplication>

using KIOTShared::Entities::BinarySensor;
using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger,PLUGIN_NAME)

ShortcutPlugin::ShortcutPlugin(QObject *parent)
    : QObject(parent)
{
}

QString ShortcutPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString ShortcutPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl ShortcutPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber ShortcutPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool ShortcutPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasKGlobalAccelTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.kde.kglobalaccel", "talk");
        if (!hasKGlobalAccelTalk) {
            qCWarning(plugin_logger) << "Missing required Flatpak D-Bus permission to talk to org.kde.kglobalaccel.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --talk-name=org.kde.kglobalaccel " + appId;
            return false;
        }
    }
    return true;
}

bool ShortcutPlugin::enabledByDefault()
{
    if (!checkCompatibility()) {
        return false;
    }
    return true; 
}
bool ShortcutPlugin::startPlugin()
{
    if(m_shortcut)
        stopPlugin();

    m_shortcut = new Shortcut(this);
    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool ShortcutPlugin::stopPlugin()
{
    if(m_shortcut)
    {
        m_shortcut->deleteLater();
        m_shortcut = nullptr;
    }
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}

#include "shortcutplugin.moc"