// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/sensor.h"

#include <Solid/Battery>
#include <Solid/Device>
#include <Solid/DeviceInterface>
#include <Solid/DeviceNotifier>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(batter)
Q_LOGGING_CATEGORY(batter, "integration.Battery")


// Helper functions to map the types to human strings
static QString mapBatteryTechnology(Solid::Battery::Technology tech)
{
    switch (tech) {
    case Solid::Battery::LithiumIon:
        return "Lithium Ion";
    case Solid::Battery::LithiumPolymer:
        return "Lithium Polymer";
    case Solid::Battery::LithiumIronPhosphate:
        return "Lithium Iron Phosphate";
    case Solid::Battery::LeadAcid:
        return "Lead Acid";
    case Solid::Battery::NickelCadmium:
        return "Nickel Cadmium";
    case Solid::Battery::NickelMetalHydride:
        return "Nickel Metal Hydride";
    default:
        return "Unknown";
    }
}

static QString mapBatteryType(Solid::Battery::BatteryType type)
{
    switch (type) {
    case Solid::Battery::PrimaryBattery:
        return "Primary Battery";
    case Solid::Battery::UpsBattery:
        return "UPS Battery";
    case Solid::Battery::MonitorBattery:
        return "Monitor Battery";
    case Solid::Battery::MouseBattery:
        return "Mouse Battery";
    case Solid::Battery::KeyboardBattery:
        return "Keyboard Battery";
    case Solid::Battery::KeyboardMouseBattery:
        return "Keyboard/Mouse Battery";
    case Solid::Battery::GamingInputBattery:
        return "Gamepad Battery";
    case Solid::Battery::BluetoothBattery:
        return "Bluetooth Battery";
    case Solid::Battery::HeadsetBattery:
        return "Headset Battery";
    case Solid::Battery::HeadphoneBattery:
        return "Headphone Battery";
    case Solid::Battery::CameraBattery:
        return "Camera Battery";
    case Solid::Battery::PhoneBattery:
        return "Phone Battery";
    case Solid::Battery::TabletBattery:
        return "Tablet Battery";
    case Solid::Battery::TouchpadBattery:
        return "Touchpad Battery";
    case Solid::Battery::PdaBattery:
        return "PDA Battery";
    default:
        return "Unknown";
    }
}

static QString mapChargeState(Solid::Battery::ChargeState state)
{
    switch (state) {
    case Solid::Battery::Charging:
        return "Charging";
    case Solid::Battery::Discharging:
        return "Discharging";
    case Solid::Battery::FullyCharged:
        return "Fully Charged";
    case Solid::Battery::NoCharge:
        return "No Charge";
    default:
        return "Unknown";
    }
}

class BatteryWatcher : public QObject
{
    Q_OBJECT
public:
    explicit BatteryWatcher(QObject *parent = nullptr);

private slots:
    void deviceAdded(const QString &udi);
    void deviceRemoved(const QString &udi);

private:
    void setupSolidWatching();
    void registerBattery(const QString &udi);
    void updateBatteryAttributes(const QString &udi);
    QHash<QString, Sensor *> m_udiToSensor;
};

BatteryWatcher::BatteryWatcher(QObject *parent)
    : QObject(parent)
{
    setupSolidWatching();
}

void BatteryWatcher::setupSolidWatching()
{
    // Watch for device changes
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded, this, &BatteryWatcher::deviceAdded);
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved, this, &BatteryWatcher::deviceRemoved);

    // Find existing batteries
    const QList<Solid::Device> batteries = Solid::Device::listFromType(Solid::DeviceInterface::Battery);
    for (const Solid::Device &device : batteries) {
        registerBattery(device.udi());
    }
    qCInfo(batter) << "Found" << batteries.count() << "battery devices";
}

void BatteryWatcher::deviceAdded(const QString &udi)
{
    Solid::Device device(udi);
    if (device.is<Solid::Battery>()) {
        qCDebug(batter) << "Battery added:" << device.displayName();
        registerBattery(udi);
    }
}

void BatteryWatcher::deviceRemoved(const QString &udi)
{
    auto it = m_udiToSensor.find(udi);
    if (it != m_udiToSensor.end()) {
        qCDebug(batter) << "Battery removed:" << udi;
        // TODO find a way to set sensor as unavailable when battery disconnects so HA shows the correct state of the battery
        it.value()->deleteLater();
        m_udiToSensor.erase(it);
    }
}

void BatteryWatcher::registerBattery(const QString &udi)
{
    Solid::Device device(udi);
    Solid::Battery *battery = device.as<Solid::Battery>();

    if (!battery) {
        qCWarning(batter) << "Device is not a battery:" << udi;
        return;
    }
    QString udi_e = udi;
    // Create display name
    QString name = device.displayName();
    if (name.isEmpty()) {
        name = device.vendor() + " " + device.product();
    }
    if (name.trimmed().isEmpty()) {
        name = "Battery " + udi.split('/').last();
    }

    // Create sensor
    Sensor *sensor = new Sensor(this);
    sensor->setDiscoveryConfig("device_class", "battery");
    sensor->setDiscoveryConfig("unit_of_measurement", "%");
    sensor->setId("battery_" + name.replace(' ', '_'));
    sensor->setName(name);

    // Set initial state and attributes
    sensor->setState(QString::number(battery->chargePercent()));

    // Connect to battery signals
    connect(battery, &Solid::Battery::chargePercentChanged, this, [this, udi](int) {
        updateBatteryAttributes(udi);
    });
    connect(battery, &Solid::Battery::chargeStateChanged, this, [this, udi](int) {
        updateBatteryAttributes(udi);
    });
    connect(battery, &Solid::Battery::energyChanged, this, [this, udi](double) {
        updateBatteryAttributes(udi);
    });
    connect(battery, &Solid::Battery::energyRateChanged, this, [this, udi](double) {
        updateBatteryAttributes(udi);
    });
    connect(battery, &Solid::Battery::voltageChanged, this, [this, udi](double) {
        updateBatteryAttributes(udi);
    });
    connect(battery, &Solid::Battery::temperatureChanged, this, [this, udi](double) {
        updateBatteryAttributes(udi);
    });
    connect(battery, &Solid::Battery::timeToEmptyChanged, this, [this, udi](qlonglong) {
        updateBatteryAttributes(udi);
    });
    connect(battery, &Solid::Battery::timeToFullChanged, this, [this, udi](qlonglong) {
        updateBatteryAttributes(udi);
    });

    m_udiToSensor[udi] = sensor;
    updateBatteryAttributes(udi);
    qCInfo(batter) << "Registered battery:" << name << "at" << battery->chargePercent() << "%";
}

void BatteryWatcher::updateBatteryAttributes(const QString &udi)
{
    auto it = m_udiToSensor.find(udi);
    if (it == m_udiToSensor.end())
        return;
    Solid::Device device(udi);
    Solid::Battery *battery = device.as<Solid::Battery>();
    if (!battery)
        return;

    QString chargeStateString = mapChargeState(battery->chargeState());
    QString batteryTypeString = mapBatteryType(battery->type());
    QString technologyString = mapBatteryTechnology(battery->technology());
    QVariantMap attributes = QVariantMap();
    if (chargeStateString != "Unknown")
        attributes["charge_state"] = chargeStateString;
    if (batteryTypeString != "Unknown")
        attributes["battery_type"] = batteryTypeString;
    if (technologyString != "Unknown")
        attributes["technology"] = technologyString;

    attributes["rechargeable"] = battery->isRechargeable();
    attributes["udi"] = udi;

    if (battery->energy() > 0)
        attributes["energy"] = battery->energy();
    if (battery->energyRate() > 0)
        attributes["energy_rate"] = battery->energyRate();
    if (battery->temperature() > 0)
        attributes["temperature"] = battery->temperature();
    if (battery->voltage() > 0)
        attributes["voltage"] = battery->voltage();
    if (!device.product().isEmpty())
        attributes["product"] = device.product();
    if (!device.vendor().isEmpty())
        attributes["vendor"] = device.vendor();
    if (!device.as<Solid::Battery>()->serial().isEmpty())
        attributes["serial"] = device.as<Solid::Battery>()->serial();
    attributes["plugged_in"] = battery->isPowerSupply();

    // Add time estimates if available
    if (battery->timeToEmpty() > 0) {
        attributes["time_to_empty_seconds"] = battery->timeToEmpty();
        attributes["time_to_empty_hours"] = QString::number(battery->timeToEmpty() / 3600.0, 'f', 1);
    }

    if (battery->timeToFull() > 0) {
        attributes["time_to_full_seconds"] = battery->timeToFull();
        attributes["time_to_full_hours"] = QString::number(battery->timeToFull() / 3600.0, 'f', 1);
    }

    it.value()->setAttributes(attributes);
}

void setupBattery()
{
    new BatteryWatcher(qApp);
}

REGISTER_INTEGRATION("Battery", setupBattery, true)

#include "battery.moc"