// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "core.h"
#include "entities/entities.h"

#include <BluezQt/Manager>
#include <BluezQt/Adapter>
#include <BluezQt/Device>
#include <BluezQt/Battery>
#include <BluezQt/InitManagerJob>


// ==== Bluetooth devices code ==========
class BluetoothDeviceSwitch : public QObject
{
    Q_OBJECT
public:
    explicit BluetoothDeviceSwitch(const BluezQt::DevicePtr &device, QObject *parent = nullptr)
    : QObject(parent), m_device(device)
    {
        m_switch = new Switch(this);
        m_switch->setId("bluetooth_device_" + device->address().replace(":", ""));
        m_switch->setName(device->name());
        m_switch->setDiscoveryConfig("icon","mdi:bluetooth");  
        updateState();
        updateAttributes();

        // Lytt til endringer
        connect(device.data(), &BluezQt::Device::connectedChanged, this, [this](bool){
            updateState();
            updateAttributes();
        });

        connect(device.data(), &BluezQt::Device::batteryChanged, this, [this](QSharedPointer<BluezQt::Battery>){            updateState();
            updateAttributes();
        });
        connect(device.data(), &BluezQt::Device::pairedChanged, this, [this](bool){
            updateState();
            updateAttributes();
        });
        connect(device.data(), &BluezQt::Device::blockedChanged, this, [this](bool){
            updateState();
            updateAttributes();
        });
        
        connect(device.data(), &BluezQt::Device::trustedChanged, this, [this](bool){
            updateState();
            updateAttributes();
        });

        
        connect(m_switch, &Switch::stateChangeRequested, this, [this](bool requestedState){
            if (!m_device)
                return;
            if (requestedState){
                m_device->connectToDevice();
            }else {
                m_device->disconnectFromDevice();
            }
        });
      
    }

private:
    BluezQt::DevicePtr m_device;
    Switch *m_switch = nullptr;

    void updateState()
    {
        if (!m_device) return;
        if(m_device->isConnected()){
            m_switch->setHaIcon("mdi:bluetooth");
            m_switch->setState(true);
        }
        else {
            m_switch->setHaIcon("mdi:bluetooth-off");
            m_switch->setState(false);
        }
    }

    void updateAttributes()
    {
        if (!m_device) return;
        if(!m_device->isPaired()){
            // TODO set entity as unavailable in HA and delete this
            qDebug() << m_device->name() << " is not paired anymore";
            m_switch->setState(false);
        }
        QVariantMap attrs;
        attrs["MAC"] = m_device->address();
        attrs["RSSI"] = m_device->rssi();
        auto battery = m_device->battery();
        if (battery)
            attrs["Battery"] = battery->percentage(); 

        attrs["Paired"] = m_device->isPaired();
        attrs["Trusted"] = m_device->isTrusted();
        attrs["Blocked"] = m_device->isBlocked();
        m_switch->setAttributes(attrs);
    }
};



// ====== Bluetooth Adapter code ======
class BluetoothAdapterWatcher : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothAdapterWatcher(QObject *parent = nullptr);
    
private:
    Switch *m_switch = nullptr;
    BluezQt::Manager *m_manager = nullptr;
    BluezQt::AdapterPtr m_adapter;
    bool m_initialized = false;
    QMap<QString, BluetoothDeviceSwitch*> m_btSwitches;
};

BluetoothAdapterWatcher::BluetoothAdapterWatcher(QObject *parent)
    : QObject(parent)
{
    m_switch = new Switch(this);
    m_switch->setId("bluetooth_adapter");
    m_switch->setName("Bluetooth Adapter");
    m_switch->setHaIcon("mdi:bluetooth");
    m_manager = new BluezQt::Manager(this);

    // Lag init-jobben
    BluezQt::InitManagerJob *job = m_manager->init();

    connect(job, &BluezQt::InitManagerJob::result, this, [this, job]() {
        if (job->error()) {
            qWarning() << "Bluez init failed:" << job->errorText();
            m_switch->setState(false);
            return;
        }

        auto adapters = job->manager()->adapters();
        if (!adapters.isEmpty()) {
            m_adapter = adapters.first(); // Første adapter
            m_initialized = true;

            qDebug() << "Adapter:" << m_adapter->name() << "Powered:" << m_adapter->isPowered();
            m_switch->setState(m_adapter->isPowered());

            connect(m_adapter.data(), &BluezQt::Adapter::poweredChanged, this, [this](bool powered){
                if (powered){
                    m_switch->setHaIcon("mdi:bluetooth");
                } else {    
                    m_switch->setHaIcon("mdi:bluetooth-off");
                }
                m_switch->setState(powered);
            });
            // TODO figure out if its a better way to do this. I tested deviceAdded and deviceRemoved but was not what i expected
            connect(m_adapter.data(), &BluezQt::Adapter::pairableChanged, this, [this]() {
                for (const auto &dev : m_adapter->devices()) {
                    const auto key = dev->address();
                    if (dev->isPaired()) {
                        if (!m_btSwitches.contains(key)) {
                            auto sw = new BluetoothDeviceSwitch(dev, this);
                            m_btSwitches.insert(key, sw);
                       }
                    }
                     else {
                        // device er unpaired, fjern switch
                        if (m_btSwitches.contains(key)) {
                            auto *sw = m_btSwitches.take(key);
                            sw->deleteLater();
                        }
                    }
                }
            });
            for (const auto &dev : m_adapter->devices()) {
                if (dev->isPaired()) {
                    const auto key = dev->address();
                    if (!m_btSwitches.contains(key)) {
                        auto sw = new BluetoothDeviceSwitch(dev, this);
                        m_btSwitches.insert(key, sw);
                    }
                }
            }

        } 
        else {
            qWarning() << "No adapters found";
            m_switch->setState(false);
        }
    });

    job->start();

    // Koble til stateChangeRequested for HA
    connect(m_switch, &Switch::stateChangeRequested, this, [this](bool requestedState){
        if (!m_initialized || !m_adapter)
            return;

        m_adapter->setPowered(requestedState);
        qDebug() << "Set adapter powered to" << requestedState;
    });
}

// Setup-funksjon som registreres i Kiot
void setupBluetoothAdapter()
{
    new BluetoothAdapterWatcher(qApp);
}

REGISTER_INTEGRATION("Bluetooth", setupBluetoothAdapter, true)

#include "bluetooth.moc"
