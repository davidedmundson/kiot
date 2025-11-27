// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QObject>
#include <QVariantMap>
#include <QMqttSubscription>
#include <KSharedConfig>
class QMqttClient;

struct IntegrationFactory {
    QString name;
    std::function<void()> factory;
    bool onByDefault = true;  // ny flagg for default enabled
};

class HaControl : public QObject {
    Q_OBJECT
public:
    HaControl();
    ~HaControl();

    static QMqttClient *mqttClient() { return s_self->m_client; }

    static bool registerIntegrationFactory(const QString &name, std::function<void()> plugin, bool onByDefault = true);
    
private:
    void doConnect();
    void loadIntegrations(KSharedConfigPtr config);
    static QList<IntegrationFactory> s_integrations;
    static HaControl *s_self;
    QMqttClient *m_client;
};

// Macro for integrasjoner
#define REGISTER_INTEGRATION(nameStr, func, onByDefault) \
static bool dummy##func = HaControl::registerIntegrationFactory(nameStr, [](){ func(); }, onByDefault);
/**
 * @brief The Entity class is a base class for types (binary sensor, sensor, etc)
 */
class Entity: public QObject
{
    Q_OBJECT
public:
    void setId(const QString &newId);
    QString id() const;

    void setName(const QString &newName);
    QString name() const;

    void setDiscoveryConfig(const QString &key, const QVariant &value);

    Entity(QObject *parent);
    QString hostname() const;
    QString baseTopic() const;

protected:
    /**
     * Called on MQTT connect, it may be called more than once
     */
    virtual void init();
    void sendRegistration();
    void setHaType(const QString &newHaType);
    QString haType() const;
    void setHaConfig(const QVariantMap &newHaConfig);

private:
    QString m_id;
    QString m_name;
    QString m_haType;
    QVariantMap m_haConfig;
};

class BinarySensor : public Entity
{
    Q_OBJECT
public:
    BinarySensor(QObject *parent = nullptr);
    void setState(bool state);
    bool state() const;
protected:
    void init() override;
private:
    void publish();
    bool m_state = false;
};

class Sensor : public Entity
{
    Q_OBJECT
public:
    Sensor(QObject *parent = nullptr);

    void setState(const QString &state);
    void setAttributes(const QVariantMap &attrs);

protected:
    void init() override;

private:
    QString m_state;
    QVariantMap m_attributes;

    void publishState();
    void publishAttributes();
};


class Event : public Entity
{
    Q_OBJECT
public:
    Event(QObject *parent = nullptr);
    void trigger();
protected:
    void init() override;
};

class Button : public Entity
{
    Q_OBJECT
public:
    Button(QObject *parent = nullptr);
Q_SIGNALS:
    void triggered();
protected:
    void init() override;
private:
    QScopedPointer<QMqttSubscription> m_subscription;
};

class Switch : public Entity
{
    Q_OBJECT
public:
    Switch(QObject *parent = nullptr);
    void setState(bool state);
Q_SIGNALS:
    void stateChangeRequested(bool state);
protected:
    void init() override;
private:
    bool m_state = false;
    QScopedPointer<QMqttSubscription> m_subscription;
};

class Number : public Entity
{
    Q_OBJECT
public:
    Number(QObject *parent = nullptr);
    void setValue(int value);
    int getValue();
// Optional customization for integrations before init()
    void setRange(int min, int max, int step = 1, const QString &unit = "%");

protected:
    void init() override;
    
Q_SIGNALS:
    void valueChangeRequested(int value);

private:
    int m_value = 0;
    int m_min = 0;
    int m_max = 100;
    int m_step = 1;
    QString m_unit = "%";

    QScopedPointer<QMqttSubscription> m_subscription;
};

