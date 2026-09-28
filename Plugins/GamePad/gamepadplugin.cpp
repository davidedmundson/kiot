#include "gamepadplugin.h"
#include <QCoreApplication>

using KIOTShared::Entities::BinarySensor;
using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger,Gamepad)

GamepadPlugin::GamepadPlugin(QObject *parent)
    : QObject(parent)
{
}

QString GamepadPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString GamepadPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl GamepadPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber GamepadPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool GamepadPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        

        bool hasInputDevice = PlatformHelper::checkFlatpakFeature("Context", "devices", "input");
        if (!hasInputDevice) {
            qCWarning(plugin_logger) << "Missing required Flatpak permission for input devices (--device=input).";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --device=input " + appId;
            return false; 
        }
    }
    
    // Sjekk om udev faktisk lar seg initialisere
    struct udev *udev = udev_new();
    if (!udev) {
        qCWarning(plugin_logger) << "Failed to initialize udev context";
        return false;
    }
    udev_unref(udev);

    return true;
}

bool GamepadPlugin::enabledByDefault()
{
    return checkCompatibility();
}

bool GamepadPlugin::startPlugin()
{
    if(m_gamepad)
        stopPlugin();

    m_gamepad = new Gamepad(this);
    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool GamepadPlugin::stopPlugin()
{
    if(m_gamepad)
    {
        m_gamepad->deleteLater();
        m_gamepad = nullptr;
    }
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}

#include "gamepadplugin.moc"