// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/entities.h"

#include <Solid/DeviceNotifier>
#include <Solid/Device>
#include <Solid/Battery>
#include <Solid/DeviceInterface>

class BatteryWatcher : public QObject
{
    Q_OBJECT
public:
    explicit BatteryWatcher(QObject *parent = nullptr);

private slots:
    void deviceAdded(const QString &udi);
    void deviceRemoved(const QString &udi);
    void batteryChargeChanged(int chargePercent, const QString &udi);
    void batteryStateChanged(int state, const QString &udi);
    void batteryDoubleChanged(double value, const QString &udi);
    void batteryLongLongChanged(qlonglong value, const QString &udi);
private:
    void setupSolidWatching();
    void registerBattery(const QString &udi);
    void updateBatteryAttributes(const QString &udi);
    QMap<QString, Sensor *> m_udiToSensor;
};

BatteryWatcher::BatteryWatcher(QObject *parent)
    : QObject(parent)
{
    setupSolidWatching();
}

void BatteryWatcher::setupSolidWatching()
{
    // Watch for device changes
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded,
            this, &BatteryWatcher::deviceAdded);
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved,
            this, &BatteryWatcher::deviceRemoved);

    // Find existing batteries
    const QList<Solid::Device> batteries = Solid::Device::listFromType(Solid::DeviceInterface::Battery);
    for (const Solid::Device &device : batteries) {
        registerBattery(device.udi());
    }
    
    qDebug() << "BatteryWatcher: Found" << batteries.count() << "battery devices";
}

void BatteryWatcher::deviceAdded(const QString &udi)
{
    Solid::Device device(udi);
    if (device.is<Solid::Battery>()) {
        qDebug() << "Battery added:" << device.displayName();
        registerBattery(udi);
    }
}

void BatteryWatcher::deviceRemoved(const QString &udi)
{
    //Do i need to disconnec the battery that was used in "connect" or will this be enough?
    auto it = m_udiToSensor.find(udi);
    if (it != m_udiToSensor.end()) {
        qDebug() << "Battery removed:" << udi;
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
        qWarning() << "Device is not a battery:" << udi;
        return;
    }
    QString udi_e = udi;
    // Create display name
    QString name = device.displayName();
    if (name.isEmpty()) {
        name = device.vendor() + " " + device.product();
    }
    if (name.trimmed().isEmpty()) {
        name = "Battery " + udi.split("/").last();
    }

    // Create sensor
    Sensor *sensor = new Sensor(this);
    sensor->setDiscoveryConfig("device_class", "battery");
    sensor->setDiscoveryConfig("unit_of_measurement", "%");
    sensor->setId("battery_" + udi_e.replace("/", "_").replace(":", "_"));
    sensor->setName(name);
 
    
    // Set initial state and attributes
    sensor->setState(QString::number(battery->chargePercent()));

    
    // Connect to battery signals
    connect(battery, &Solid::Battery::chargePercentChanged,
            this, [this, udi](int chargePercent) {
                batteryChargeChanged(chargePercent, udi);
    });
            
    connect(battery, &Solid::Battery::chargeStateChanged,
            this, [this, udi](int state) {
                batteryStateChanged(state, udi);
    });

    if (battery->energy() > 0) {
        connect(battery, &Solid::Battery::energyChanged,
            this, [this, udi](double value) {
                batteryDoubleChanged(value, udi);
        });
    }
    if (battery->energyRate() > 0) {
        connect(battery, &Solid::Battery::energyRateChanged,
            this, [this, udi](double value) {
                batteryDoubleChanged(value, udi);
        });
    }
    if (battery->voltage() > 0) {
        connect(battery, &Solid::Battery::voltageChanged,
            this, [this, udi](double value) {
                batteryDoubleChanged(value, udi);
        });
    }
    if (battery->temperature() > 0) {
        connect(battery, &Solid::Battery::temperatureChanged,
            this, [this, udi](double value) {
                batteryDoubleChanged(value, udi);
        });
    }
    
    if (battery->timeToEmpty() > 0) {
        connect(battery, &Solid::Battery::timeToEmptyChanged,
            this, [this, udi](qlonglong value) {
                batteryLongLongChanged(value, udi);
        });
    }
    
    if (battery->timeToFull() > 0) {
        connect(battery, &Solid::Battery::timeToFullChanged,
            this, [this, udi](qlonglong value) {
                batteryLongLongChanged(value, udi);
        });
    }
    
    m_udiToSensor[udi] = sensor;
    updateBatteryAttributes(udi);
    qDebug() << "Registered battery:" << name << "at" << battery->chargePercent() << "%";
}

void BatteryWatcher::batteryDoubleChanged(double value, const QString &udi)
{
    Q_UNUSED(value);
    auto it = m_udiToSensor.find(udi);
    if (it != m_udiToSensor.end()) {
        updateBatteryAttributes(udi);
    }
}


void BatteryWatcher::batteryLongLongChanged(qlonglong value, const QString &udi)
{
    Q_UNUSED(value);
    auto it = m_udiToSensor.find(udi);
    if (it != m_udiToSensor.end()) {
        updateBatteryAttributes(udi);
    }
} 
void BatteryWatcher::batteryChargeChanged(int chargePercent, const QString &udi)
{
    auto it = m_udiToSensor.find(udi);
    if (it != m_udiToSensor.end()) {
        it.value()->setState(QString::number(chargePercent));
        updateBatteryAttributes(udi);
    }
}

void BatteryWatcher::batteryStateChanged(int state, const QString &udi)
{
    Q_UNUSED(state)
    updateBatteryAttributes(udi);
}

void BatteryWatcher::updateBatteryAttributes(const QString &udi)
{
    auto it = m_udiToSensor.find(udi);
    if (it == m_udiToSensor.end()) return;
    Solid::Device device(udi);
    Solid::Battery *battery = device.as<Solid::Battery>();
    if (!battery) return;

    // Map Solid::Battery::ChargeState enum to readable strings
    QString chargeStateString;
    switch (battery->chargeState()) {
        case Solid::Battery::Charging:
            chargeStateString = "Charging";
            break;
        case Solid::Battery::Discharging:
            chargeStateString = "Discharging";
            break;
        case Solid::Battery::FullyCharged:
            chargeStateString = "Fully Charged";
            break;
        case Solid::Battery::NoCharge:
            chargeStateString = "No Charge";
            break;       
        default:
            chargeStateString = "Unknown";
            break;
    }

    // Map Solid::Battery::BatteryType enum to readable strings
    QString batteryTypeString;
    switch (battery->type()) {
        case Solid::Battery::PrimaryBattery:
            batteryTypeString = "Primary Battery";
            break;
        case Solid::Battery::UpsBattery:
            batteryTypeString = "UPS Battery";
            break;
        case Solid::Battery::MonitorBattery:
            batteryTypeString = "Monitor Battery";
            break;
        case Solid::Battery::MouseBattery:
            batteryTypeString = "Mouse Battery";
            break;
        case Solid::Battery::KeyboardBattery:
            batteryTypeString = "Keyboard Battery";
            break;
        case Solid::Battery::KeyboardMouseBattery:
            batteryTypeString = "Keyboard/Mouse Battery";
            break;
        case Solid::Battery::GamingInputBattery:
            batteryTypeString = "Gamepad Battery";
            break;
        case Solid::Battery::BluetoothBattery:
            batteryTypeString = "Bluetooth Battery";
            break;
        case Solid::Battery::HeadsetBattery:
            batteryTypeString = "Headset Battery";
            break;
        case Solid::Battery::HeadphoneBattery:
            batteryTypeString = "Headphone Battery";
            break;
        case Solid::Battery::CameraBattery:
            batteryTypeString = "Camera Battery";
            break;
        case Solid::Battery::PhoneBattery:
            batteryTypeString = "Phone Battery";
            break;
        case Solid::Battery::TabletBattery:
            batteryTypeString = "Tablet Battery";
            break;
        case Solid::Battery::TouchpadBattery:
            batteryTypeString = "Touchpad Battery";
            break;
        case Solid::Battery::PdaBattery:
            batteryTypeString = "PDA Battery";
            break;
        default:
            batteryTypeString = "Unknown";
            break;
    }
    QString technologyString;
    switch (battery->technology()) {
        case  Solid::Battery::LithiumIon:
            technologyString = "Lithium Ion";
            break;
        case Solid::Battery::LithiumPolymer:
            technologyString = "Lithium Polymer";
            break;
        case Solid::Battery::LithiumIronPhosphate:
            technologyString = "Lithium Iron Phosphate";
            break;
        case Solid::Battery::LeadAcid:
            technologyString = "Lead Acid";
            break;
        case Solid::Battery::NickelCadmium:
            technologyString = "Nickel cadmium";
            break;
        case Solid::Battery::NickelMetalHydride:
            technologyString = "Nickel metal hydride";
            break;
        default:
            technologyString = "Unknown";
            break;
    }
    QVariantMap attributes = QVariantMap();
    attributes["charge_state"] = chargeStateString;
    attributes["battery_type"] = batteryTypeString;
    attributes["rechargeable"] = battery->isRechargeable();
    attributes["udi"] = udi;
    attributes["technology"] = technologyString;
  
    if(battery->energy() > 0)
        attributes["energy"] = battery->energy();
    if(battery->energyRate() > 0)
        attributes["energy_rate"] = battery->energyRate();
    if(battery->temperature() > 0)
        attributes["temperature"] = battery->temperature();
    if(battery->voltage() > 0)
        attributes["voltage"] = battery->voltage();
    if (!device.product().isEmpty())
        attributes["product"] = device.product();
    if ( !device.vendor().isEmpty())
       attributes["vendor"] = device.vendor();
    if ( !device.as<Solid::Battery>()->serial().isEmpty())
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