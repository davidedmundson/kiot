// SPDX-FileCopyrightText: 2026 Kloud <dgudim@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "customsensorplugin.h"
#include "customsensor.h"
#include <KSharedConfig>
#include <KConfigGroup>
using KIOTShared::Entities::BinarySensor;
using KIOTShared::PlatformHelper;

DEFINE_LOGGER(plugin_logger,CustomSensors)

constexpr qint64 DefaultIntervalMs = 10 * 1000;
CustomSensorPlugin::CustomSensorPlugin(QObject *parent)
    : QObject(parent)
{
}

QString CustomSensorPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString CustomSensorPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl CustomSensorPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber CustomSensorPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool CustomSensorPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasFlatpakTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.freedesktop.Flatpak", "talk");
        if (!hasFlatpakTalk) {
            qCWarning(plugin_logger) << "Missing required Flatpak permission to talk to org.freedesktop.Flatpak.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --talk-name=org.freedesktop.Flatpak " + appId;
            return false;
        }
    }
    return true;
}
bool CustomSensorPlugin::enabledByDefault()
{
    if(!checkCompatibility())
        return false;

    return true;
}


bool CustomSensorPlugin::startPlugin()
{
    if(!checkCompatibility())
    {
        qCWarning(plugin_logger) << name() << " plugin is not compatible with this system";
        return false;
    }


    auto sensorConfigToplevel = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig )->group("CustomSensors");
    const QStringList sensorIds = sensorConfigToplevel.groupList();
    int loaded = 0;
    for (const QString &sensorId : sensorIds) {
        const KConfigGroup group = sensorConfigToplevel.group(sensorId);

        const QString command = group.readEntry("command");
        if (command.isEmpty()) {
            qCWarning(plugin_logger) << "Skipping custom sensor '" << sensorId << "'. Missing command";
            continue;
        }

        const QString name = group.readEntry("name", sensorId);

        qint64 intervalMs = DefaultIntervalMs;
        const QString intervalStr = group.readEntry("interval");
        if (!intervalStr.isEmpty()) {
            const qint64 parsedMs = parseTimeSpanToMs(intervalStr);
            if (parsedMs > 0) {
                intervalMs = parsedMs;
            } else {
                qCWarning(plugin_logger) << "Failed to parse interval '" << intervalStr << "' for custom sensor '" << sensorId << "'. Using default";
            }
        }

        auto customSensor = new CustomSensor(sensorId, name, command, intervalMs, this);
        Sensor *sensor = customSensor->sensor();

        const QString deviceClass = group.readEntry("device_class");
        if (!deviceClass.isEmpty()) {
            sensor->setDiscoveryConfig("device_class", deviceClass);
        }
        const QString unit = group.readEntry("unit_of_measurement");
        if (!unit.isEmpty()) {
            sensor->setDiscoveryConfig("unit_of_measurement", unit);
        }
        const QString stateClass = group.readEntry("state_class");
        if (!stateClass.isEmpty()) {
            sensor->setDiscoveryConfig("state_class", stateClass);
        }
        const QString icon = group.readEntry("icon");
        if (!icon.isEmpty()) {
            sensor->setDiscoveryConfig("icon", icon);
        }

        loaded++;
    }

    if (loaded >= 1) {
        qCInfo(plugin_logger) << "Loaded" << loaded << "custom sensor(s):" << sensorIds.join(", ");
    }
    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool CustomSensorPlugin::stopPlugin()
{
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}

#include "customsensorplugin.moc"