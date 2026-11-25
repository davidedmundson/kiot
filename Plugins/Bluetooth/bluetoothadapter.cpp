// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "bluetoothadapter.h"

#include <BluezQt/Adapter>

#include <BluezQt/Battery>
#include <BluezQt/Device>
#include <BluezQt/InitManagerJob>
#include <BluezQt/Manager>
#include <KSharedConfig>
#include <KConfigGroup>

DEFINE_PLUGIN_LOGGER(btad_logs,Bluetooth)

BluetoothAdapterWatcher::BluetoothAdapterWatcher(QObject *parent) : QObject(parent)
{
    ensureConfig();
    m_switch = new Switch(this);
    m_switch->setId("bluetooth_adapter");
    m_switch->setName("Bluetooth Adapter");
    m_switch->setDiscoveryConfig("icon", "mdi:bluetooth");
    m_manager = new BluezQt::Manager(this);

    BluezQt::InitManagerJob *job = m_manager->init();

    connect(job, &BluezQt::InitManagerJob::result, this, [this, job]() {
        if (job->error()) {
            qCWarning(btad_logs) << "Bluez init failed:" << job->errorText();
            m_switch->setState(false);
            return;
        }

        // connect to the signals for dynamic creating/removing of bluetooth devices based on paired state under runtime
        connect(m_manager, &BluezQt::Manager::deviceAdded, this, [this](const BluezQt::DevicePtr &device) {
            if(m_adapter && device->adapter() == m_adapter)
            {
                if(!device->isPaired())
                    return;
                const auto key = device->address();
                if (!m_btSwitches.contains(key)) {
                    auto sw = new BluetoothDeviceSwitch(device, this);
                    m_btSwitches.insert(key, sw);
                    qCDebug(btad_logs) <<  "Device added as switch in HA:" << device->name() << "from the deviceAdded signal";
                }
            }
        });
        connect(m_manager, &BluezQt::Manager::deviceRemoved, this, [this](const BluezQt::DevicePtr &device) {
                const auto key = device->address();
                if (m_btSwitches.contains(key)) {
                    auto *sw = m_btSwitches.take(key);
                    // not operational: bluetoothd went away and takes all devices with it, they come back with it
                    if(m_autoRemove && m_manager->isOperational())
                        sw->unregisterSwitch();
                    qCDebug(btad_logs) << "Device removed from HA (unpaired via deviceChanged):" << device->name();
                    delete sw;
                }
            
        });

        connect(m_manager, &BluezQt::Manager::deviceChanged, this, [this](const BluezQt::DevicePtr &device) {
            if (!m_adapter || device->adapter() != m_adapter)
                return;

            const auto key = device->address();
            if (device->isPaired()) {
                if (!m_btSwitches.contains(key)) {
                    auto sw = new BluetoothDeviceSwitch(device, this);
                    m_btSwitches.insert(key, sw);
                    qCDebug(btad_logs) << "Device added as switch in HA:" << device->name();
                }
            } else {
                    if (m_btSwitches.contains(key)) {
                        auto *sw = m_btSwitches.take(key);
                        if(m_autoRemove)
                            sw->unregisterSwitch();
                        qCDebug(btad_logs) << "Device removed from HA (unpaired via deviceChanged):" << device->name();
                        delete sw;
                    }
            }
        });

        // follow the adapter: bluetoothd restarting or the adapter being unplugged replaces the Adapter objects
        connect(m_manager, &BluezQt::Manager::adapterAdded, this, [this](const BluezQt::AdapterPtr &adapter) {
            if (!m_adapter)
                setAdapter(adapter);
        });
        connect(m_manager, &BluezQt::Manager::adapterRemoved, this, [this](const BluezQt::AdapterPtr &adapter) {
            if (adapter == m_adapter)
                setAdapter(m_manager->adapters().value(0));
        });

        // Use first adapter, could probably be customized from config but who has more than 1 bt adapter?
        setAdapter(job->manager()->adapters().value(0));
    });

    job->start();

    // Connect to signal from switch to adapter, so we can turn bluetooth on/off
    connect(m_switch, &Switch::stateChangeRequested, this, [this](bool requestedState) {
        if (!m_initialized || !m_adapter)
            return;

        m_adapter->setPowered(requestedState);
        qCDebug(btad_logs) << "Set adapter powered to" << requestedState;
    });
}

void BluetoothAdapterWatcher::setAdapter(const BluezQt::AdapterPtr &adapter)
{
    if (m_adapter)
        disconnect(m_adapter.data(), nullptr, this, nullptr);
    m_adapter = adapter;
    m_initialized = !m_adapter.isNull();

    if (!m_adapter) {
        qCWarning(btad_logs) << "No adapters found";
        if (m_switch->state())
            m_switch->setHaIcon("mdi:bluetooth-off");
        m_switch->setState(false);
        return;
    }
    qCInfo(btad_logs) << "Using Bluetooth adapter" << m_adapter->name() << "(" << m_adapter->address() << ")";

    // connect to adapter signals for updates
    connect(m_adapter.data(), &BluezQt::Adapter::poweredChanged, this, &BluetoothAdapterWatcher::update);
    connect(m_adapter.data(), &BluezQt::Adapter::discoverableChanged, this, &BluetoothAdapterWatcher::update);
    connect(m_adapter.data(), &BluezQt::Adapter::discoveringChanged, this, &BluetoothAdapterWatcher::update);
    connect(m_adapter.data(), &BluezQt::Adapter::nameChanged, this, &BluetoothAdapterWatcher::update);
    connect(m_adapter.data(), &BluezQt::Adapter::systemNameChanged, this, &BluetoothAdapterWatcher::update);
    connect(m_adapter.data(), &BluezQt::Adapter::uuidsChanged, this, &BluetoothAdapterWatcher::update);

    update();

    // Add all paired devices (after a bluetoothd restart they follow one by one through deviceAdded)
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

void BluetoothAdapterWatcher::update()
{
    if (!m_adapter || !m_switch) {
        qCWarning(btad_logs) << "No adapter or switch found";
        return;
    }

    bool powered = m_adapter->isPowered();
    if (powered && !m_switch->state()) {
        m_switch->setHaIcon("mdi:bluetooth");
    } else if (!powered && m_switch->state()) {
        m_switch->setHaIcon("mdi:bluetooth-off");
    }
    if (m_switch->state() != powered)
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

void BluetoothAdapterWatcher::ensureConfig()
{
    auto config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    auto group = KConfigGroup(config, "Bluetooth");
    if (group.hasKey("RemoveDevices")){
        bool value = group.readEntry("RemoveDevices", false);
        m_autoRemove = value;
        qCDebug(btad_logs) << "Setting RemoveDevices to " << value;
        return;
    }
    else{
        group.writeEntry("RemoveDevices", true);
        m_autoRemove = true;
        config->sync();
        qCDebug(btad_logs) << "Config was empty, writing default RemoveDevices as true";
        return;

    }

}