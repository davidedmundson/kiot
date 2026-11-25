// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "bluetoothdeviceswitch.h"

#include <QObject>

#include <BluezQt/Adapter>

#include <BluezQt/InitManagerJob>
#include <BluezQt/Manager>

#include <KIOTShared/kiotshared.h>

using KIOTShared::Entities::Switch;
using KIOTShared::PlatformHelper;
// ====== Bluetooth Adapter code ======
class BluetoothAdapterWatcher : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothAdapterWatcher(QObject *parent = nullptr);

private:
    void ensureConfig();
    void setAdapter(const BluezQt::AdapterPtr &adapter);
    void update();

    Switch *m_switch = nullptr;
    BluezQt::Manager *m_manager = nullptr;
    BluezQt::AdapterPtr m_adapter;
    bool m_initialized = false;
    bool m_autoRemove = false;
    QMap<QString, BluetoothDeviceSwitch *> m_btSwitches;
};