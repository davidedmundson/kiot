// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

// This entity is based on this integration https://www.home-assistant.io/integrations/device_trigger.mqtt/
#include "event.h"
#include "core.h"
#include <QMqttClient>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(ev)
Q_LOGGING_CATEGORY(ev, "entities.Event")

Event::Event(QObject *parent)
    : Entity(parent)
{
}

QString Event::triggerType() const
{
    return m_triggerType;
}

void Event::setTriggerType(const QString &type)
{
    if (m_triggerType != type) {
        m_triggerType = type;
        emit triggerTypeChanged();
    }
}

QString Event::triggerSubtype() const
{
    return m_triggerSubtype;
}

void Event::setTriggerSubtype(const QString &subtype)
{
    if (m_triggerSubtype != subtype) {
        m_triggerSubtype = subtype;
        emit triggerSubtypeChanged();
    }
}

QStringList Event::availableTriggerTypes() const
{
    return {
        "button_short_press",
        "button_short_release",
        "button_long_press", 
        "button_long_release",
        "button_double_press",
        "button_triple_press",
        "button_quadruple_press",
        "button_quintuple_press"
    };
}

QStringList Event::availableTriggerSubtypes() const
{
    return {
        "turn_on",
        "turn_off", 
        "button_1",
        "button_2",
        "button_3",
        "button_4",
        "button_5",
        "button_6"
    };
}

void Event::init()
{
    setHaType("device_automation");
    setDiscoveryConfig("automation_type", "trigger");
    setDiscoveryConfig("topic", baseTopic());
    setDiscoveryConfig("type", triggerType());
    QString subtype = triggerSubtype();
    if (!subtype.isEmpty()) {
        setDiscoveryConfig("subtype", subtype);
    }
    sendRegistration();
}

void Event::trigger()
{
    triggerWithPayload(m_triggerType);
}

void Event::triggerCustom(const QString &customType) {
    triggerWithPayload(customType);
}

void Event::triggerWithPayload(const QString &payload)
{
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), payload.toUtf8(), 0, false);
        HaControl::mqttClient()->publish(baseTopic(), "", 0, false);
    }
}