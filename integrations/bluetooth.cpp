// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "core.h"
#include "entities/entities.h"

#include <BluezQt/Adapter>
// Re add after bluez-qt works on new version via flatpak manifest
//#include <BluezQt/Battery>
#include <BluezQt/Device>
#include <BluezQt/InitManagerJob>
#include <BluezQt/Manager>


// ==== Bluetooth devices code ==========
class BluetoothDeviceSwitch : public QObject
{
    Q_OBJECT
public:
    explicit BluetoothDeviceSwitch(const BluezQt::DevicePtr &device, QObject *parent = nullptr)
    : QObject(parent), m_device(device)
    {
        m_switch = new Switch(this);
        m_switch->setId("bluetooth_device_" + device->address().replace(':', '_'));
        m_switch->setName(device->name());
        m_switch->setDiscoveryConfig("icon","mdi:bluetooth");  
        update();

        // Connect signals
        connect(device.data(), &BluezQt::Device::connectedChanged, this, [this](bool){
            update();
        });
        connect(device.data(), &BluezQt::Device::batteryChanged, this, [this](QSharedPointer<BluezQt::Battery>){            
            update();
        });
        connect(device.data(), &BluezQt::Device::pairedChanged, this, [this](bool){
            update();
        });
        connect(device.data(), &BluezQt::Device::blockedChanged, this, [this](bool){
            update();
        });
        connect(device.data(), &BluezQt::Device::trustedChanged, this, [this](bool){
            update();
        });
        // connect to signal from switch in HA        
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


    void update()
    {
        if (!m_device) return;
        if(!m_device->isPaired()){
            // TODO set entity as unavailable in HA and delete this
            qDebug() << m_device->name() << " is not paired anymore";
            m_switch->setState(false);
        }
        //Only update state and icon if actually changed to avoid unnecessary re registreations with mqtt
        if(m_device->isConnected() && !m_switch->state())
        {
            m_switch->setHaIcon("mdi:bluetooth");
            m_switch->setState(true);
        }
        else if(!m_device->isConnected() && m_switch->state()) 
        {
            m_switch->setHaIcon("mdi:bluetooth-off");
            m_switch->setState(false);
        }
        //Update attributes
        QVariantMap attrs;
        attrs["mac"] = m_device->address();
        attrs["rssi"] = m_device->rssi();
        // Re add after bluez-qt works on new version via flatpak manifest
        //auto battery = m_device->battery();
        //if (battery)
        //    attrs["battery"] = battery->percentage(); 

        attrs["paired"] = QVariant(m_device->isPaired()).toString();
        attrs["trusted"] = QVariant(m_device->isTrusted()).toString();
        attrs["blocked"] = QVariant(m_device->isBlocked()).toString();
        if (m_switch->attributes() != attrs)
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
    void update();
    void CheckPairedState();
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
    m_switch->setDiscoveryConfig("icon", "mdi:bluetooth");
    m_manager = new BluezQt::Manager(this);

    // create the init job
    BluezQt::InitManagerJob *job = m_manager->init();

    connect(job, &BluezQt::InitManagerJob::result, this, [this, job]() {
        if (job->error()) {
            qWarning() << "Bluez init failed:" << job->errorText();
            m_switch->setState(false);
            return;
        }

        auto adapters = job->manager()->adapters();
        if (!adapters.isEmpty()) {
            m_adapter = adapters.first(); // Use first adapter, could probably be customized from config but who has more than 1 bt adapter?
            m_initialized = true;
            update();
            // connect to adapter signals for updates
            connect(m_adapter.data(), &BluezQt::Adapter::poweredChanged, this, &BluetoothAdapterWatcher::update);
            connect(m_adapter.data(), &BluezQt::Adapter::discoverableChanged, this, &BluetoothAdapterWatcher::update);
            connect(m_adapter.data(), &BluezQt::Adapter::discoveringChanged, this, &BluetoothAdapterWatcher::update);
            connect(m_adapter.data(), &BluezQt::Adapter::nameChanged, this, &BluetoothAdapterWatcher::update);
            connect(m_adapter.data(), &BluezQt::Adapter::systemNameChanged, this, &BluetoothAdapterWatcher::update);
            connect(m_adapter.data(), &BluezQt::Adapter::uuidsChanged, this, &BluetoothAdapterWatcher::update);

              
            // TODO figure out if its a better way to do this check for new/removed paired devices. 
            // I tested deviceAdded and deviceRemoved but was not what i expected
            connect(m_adapter.data(), &BluezQt::Adapter::deviceAdded, this, [this]() {
                CheckPairedState();
                update();
            });
            connect(m_adapter.data(), &BluezQt::Adapter::deviceRemoved, this, [this]() {
                CheckPairedState();
                update();
            });
            
            // Add all paired devices 
            // could probably use the CheckPairedState function here now
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

    // Connect to signal from switch to adapter, so we can turn bluetooth on/off
    connect(m_switch, &Switch::stateChangeRequested, this, [this](bool requestedState){
        if (!m_initialized || !m_adapter)
            return;

        m_adapter->setPowered(requestedState);
        qDebug() << "Set adapter powered to" << requestedState;
    });
}
void BluetoothAdapterWatcher::CheckPairedState()
{
    if(!m_adapter) return;

    for (const auto &dev : m_adapter->devices()) {
        const auto key = dev->address();
            if (dev->isPaired()) {
                if (!m_btSwitches.contains(key)) {
                    auto sw = new BluetoothDeviceSwitch(dev, this);
                    m_btSwitches.insert(key, sw);
                    }
            }
            else {
            // device is no longer paired, remove the switch if it exists
            //Does anyone know how to actually unpair? I can't find anything making paired state change, forget from settings does not work
                if (m_btSwitches.contains(key)) {
                    auto *sw = m_btSwitches.take(key);
                    sw->deleteLater();
                }
            }
    }
}
void BluetoothAdapterWatcher::update(){
    if(!m_adapter || !m_switch)
    {

    qDebug() << "No adapter or switch found"; //Deb
    return;
    }

    //Adapter state
    //Only change icon and state if its actually not matching HA
    bool powered =  m_adapter->isPowered();  
    if (powered && !m_switch->state()){
        m_switch->setHaIcon("mdi:bluetooth");
    } else if (!powered && m_switch->state()) {    
        m_switch->setHaIcon("mdi:bluetooth-off");
    }
    if(m_switch->state() != powered)
        m_switch->setState(powered);
    QVariantMap attrs;
    attrs["mac"] = m_adapter->address();
    attrs["name"] = m_adapter->name();
    attrs["system_name"] = m_adapter->systemName();
    attrs["discovering"] = QVariant(m_adapter->isDiscovering()).toString();
    attrs["discoverable"] = QVariant(m_adapter->isDiscoverable()).toString();
    attrs["pairable"] = QVariant(m_adapter->isPairable()).toString();
    QVariantList uuidList;
    for (const auto &u : m_adapter->uuids())
        uuidList.append(u);
    attrs["uuids"] = uuidList;
    if (m_switch->attributes() != attrs)
        m_switch->setAttributes(attrs);

}
// setup function
void setupBluetoothAdapter()
{
    new BluetoothAdapterWatcher(qApp);
}

REGISTER_INTEGRATION("Bluetooth", setupBluetoothAdapter, true)

#include "bluetooth.moc"
