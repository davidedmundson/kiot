// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "dbusproperty.h"
#include "entities/switch.h"
#include <QApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(dnd)
Q_LOGGING_CATEGORY(dnd, "integration.DnD")

class DnDManager : public QObject {
    Q_OBJECT
public:
    DnDManager(QObject *parent = nullptr) : QObject(parent) {
        m_switch = new Switch(this);
        m_switch->setId("dnd");
        m_switch->setName("Do not disturb");
        m_switch->setDiscoveryConfig("icon", "mdi:bell-cancel");

        m_dndProperty = new DBusProperty("org.freedesktop.Notifications", "/org/freedesktop/Notifications", 
                                        "org.freedesktop.Notifications", "Inhibited", this);
        
        m_dndInterface = new QDBusInterface("org.freedesktop.Notifications","/org/freedesktop/Notifications",
                "org.freedesktop.Notifications",QDBusConnection::sessionBus(),this);

        connect(m_dndProperty, &DBusProperty::valueChanged, this, [this](const QVariant &value) {
            m_switch->setState(value.toBool());
        });
        
        m_switch->setState(m_dndProperty->value().toBool());

        connect(m_switch, &Switch::stateChangeRequested, this, &DnDManager::onStateChangeRequested);
    }

    Q_SLOT void onStateChangeRequested(bool enabled)
    {
        if (enabled) 
        {
            QDBusReply<uint> reply = m_dndInterface->call("Inhibit", QApplication::applicationName(),
                    "Controlled by Kiot", QVariantMap());
            if (reply.isValid()) {
                m_inhibitId = reply.value();
            }
        } 
        else if (m_inhibitId > 0) {
            m_dndInterface->call("UnInhibit", m_inhibitId);
            m_inhibitId = 0;
        }

    }

private:
    Switch *m_switch;
    DBusProperty *m_dndProperty;
    QDBusInterface *m_dndInterface;
    uint m_inhibitId = 0;
};

void setupDndSensor() {
    new DnDManager(qApp);
}

REGISTER_INTEGRATION("DnD", setupDndSensor, true)

#include "dndstate.moc"