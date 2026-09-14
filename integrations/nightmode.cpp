// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "core.h"
#include "entities/entities.h"
#include <QCoreApplication>
#include "dbusproperties.h"
#include "kwinnightlight.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(nightmode)
Q_LOGGING_CATEGORY(nightmode, "integration.NightMode")

class NightMode : public QObject
{
    Q_OBJECT
public:
    NightMode(QObject *parent);
    void updateAttributes();
private:
    BinarySensor *m_sensor;
    Switch *m_switch;
    std::optional<uint32_t> m_inhibitCookie;

    OrgKdeKWinNightLightInterface *m_nightLightIface;
    OrgFreedesktopDBusPropertiesInterface *m_propsIface;
};

NightMode::NightMode(QObject *parent)
    : QObject(parent)
{
    m_sensor = new BinarySensor(this);
    m_sensor->setId("nightmode_inhibited");
    m_sensor->setName("Night Mode Inhibited");

    m_nightLightIface = new OrgKdeKWinNightLightInterface(QStringLiteral("org.kde.KWin"), QStringLiteral("/org/kde/KWin/NightLight"), QDBusConnection::sessionBus(),this);
    if (!m_nightLightIface->isValid()) {
        qCWarning(nightmode) << "Failed to connect to KWin NightLight D-Bus interface!";
    }


    m_propsIface = new OrgFreedesktopDBusPropertiesInterface(QStringLiteral("org.kde.KWin"), QStringLiteral("/org/kde/KWin/NightLight"), QDBusConnection::sessionBus(), this);
    connect(m_propsIface, &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, [this](const QString &interfaceName, const QVariantMap &changed, const QStringList &invalidated) {
        Q_UNUSED(invalidated)
        Q_UNUSED(changed)
        Q_UNUSED(interfaceName)
        m_sensor->setState(m_nightLightIface->inhibited());
        updateAttributes();
        
    });
    
    m_switch = new Switch(this);
    m_switch->setId("nightmode_inhibit");
    m_switch->setName("Night Mode Inhibit");
    QObject::connect(m_switch, &Switch::stateChangeRequested, this, [this](bool state) {
        if (state) {
            QDBusReply<uint32_t> reply = m_nightLightIface->inhibit();
            if (!reply.isValid()) {
                qCWarning(nightmode) << "Failed to inhibit nightmode";
                return;
            }
            m_inhibitCookie = reply.value();
        } else if (m_inhibitCookie.has_value()) {
            m_nightLightIface->uninhibit(m_inhibitCookie.value());
        }
        m_switch->setState(state);
    });
    m_sensor->setState(m_nightLightIface->inhibited());
    m_switch->setState(m_nightLightIface->inhibited());

    updateAttributes();


}
void NightMode::updateAttributes()
{
    if(!m_nightLightIface->isValid())
        return;
    if(!m_sensor)
        return;
    QVariantMap attributes;
    attributes["available"] = m_nightLightIface->available();
    attributes["enabled"] = m_nightLightIface->enabled();
    attributes["temperature"] = m_nightLightIface->currentTemperature();
    attributes["daylight"] = m_nightLightIface->daylight();
    attributes["mode"] = m_nightLightIface->mode();
    attributes["previousTransitionDateTime"] = m_nightLightIface->previousTransitionDateTime();
    attributes["previousTransitionDuration"] = m_nightLightIface->previousTransitionDuration();
    attributes["scheduledTransitionDateTime"] = m_nightLightIface->scheduledTransitionDateTime();
    attributes["scheduledTransitionDuration"] = m_nightLightIface->scheduledTransitionDuration();
    attributes["targetTemperature"] = m_nightLightIface->targetTemperature();
    qCDebug(nightmode) << "Updating attributes" << attributes;
    m_sensor->setAttributes(attributes);
    m_sensor->setState(m_nightLightIface->inhibited());

}
void setupNightmode()
{
    new NightMode(qApp);
}

REGISTER_INTEGRATION("Nightmode", setupNightmode, true)

#include "nightmode.moc"
