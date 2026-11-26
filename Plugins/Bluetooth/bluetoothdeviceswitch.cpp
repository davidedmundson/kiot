// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "bluetoothdeviceswitch.h"

DEFINE_PLUGIN_LOGGER(bt,Bluetooth)

BluetoothDeviceSwitch::BluetoothDeviceSwitch(const BluezQt::DevicePtr &device, QObject *parent)
    : QObject(parent)
    , m_device(device)
{
    m_switch = new KIOTShared::Entities::Switch(this);
    m_switch->setId("bluetooth_device_" + device->address().replace(':', '_'));
    m_switch->setName(device->name());
    m_switch->setDiscoveryConfig("icon", "mdi:bluetooth");
    m_switch->runtimeRegistration();

    // Connect signals
    connect(m_device.data(), &BluezQt::Device::connectedChanged, this, [this](bool) {
        update();
    });
    connect(device.data(), &BluezQt::Device::batteryChanged, this, [this](QSharedPointer<BluezQt::Battery>) {
        update();
    });
    connect(device.data(), &BluezQt::Device::pairedChanged, this, [this](bool) {
        update();
    });
    connect(device.data(), &BluezQt::Device::blockedChanged, this, [this](bool) {
        update();
    });
    connect(device.data(), &BluezQt::Device::trustedChanged, this, [this](bool) {
        update();
    });
    
    // connect to signal from switch in HA
    connect(m_switch, &KIOTShared::Entities::Switch::stateChangeRequested, this, [this](bool requestedState) {
        if (!m_device)
            return;
        if (requestedState) {
            m_device->connectToDevice();
        } else {
            m_device->disconnectFromDevice();
        }
    });
    
    update();
    qCInfo(bt) << "Bluetooth device added: " << device->name() << " (" << device->address() << ")";
}

BluetoothDeviceSwitch::~BluetoothDeviceSwitch()
{
}

void BluetoothDeviceSwitch::unregisterSwitch()
{
    if (m_switch)
        m_switch->unRegister();
}

void BluetoothDeviceSwitch::update()
{
    if (!m_device)
        return;
    if (!m_device->isPaired()) {
        // BluetoothAdapterWatcher removes this switch on the deviceChanged that follows, and unregisters it
        // from Home Assistant only with RemoveDevices=true
        qCDebug(bt) << m_device->name() << " is not paired anymore";
        return;
    }
    
    // Only update state and icon if actually changed to avoid unnecessary re registrations with mqtt
    if (m_device->isConnected() && !m_switch->state()) {
        m_switch->setHaIcon("mdi:bluetooth");
        m_switch->setState(true);
    } else if (!m_device->isConnected() && m_switch->state()) {
        m_switch->setHaIcon("mdi:bluetooth-off");
        m_switch->setState(false);
    }
    
    // Update attributes
    QVariantMap attrs;
    attrs["mac"] = m_device->address();
    attrs["rssi"] = m_device->rssi();

    auto battery = m_device->battery();
    if (battery)
        attrs["battery"] = battery->percentage();

    attrs["paired"] = QVariant(m_device->isPaired()).toString();
    attrs["trusted"] = QVariant(m_device->isTrusted()).toString();
    attrs["blocked"] = QVariant(m_device->isBlocked()).toString();
    
    if (m_switch->attributes() != attrs)
        m_switch->setAttributes(attrs);
}