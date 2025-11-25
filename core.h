#pragma once

#include <QObject>
#include <QVariantMap>
#include <QMqttSubscription>

class QMqttClient;

class HaControl : public QObject {
public:
    HaControl();
    ~HaControl();
    static QMqttClient *mqttClient() {
        return s_self->m_client;
    }

    // you probably want the macro
    static bool registerIntegrationFactory(const QFunctionPointer factory);
private:
    void doConnect();
    static QList<QFunctionPointer> s_integrations;
    static HaControl *s_self;
    QMqttClient *m_client;
    // QNetworkConfigurationManager m_networkConfigurationManager;
};



// Dave's shitty plugin system to avoid updating this file each time we add an integration
#define REGISTER_INTEGRATION(name) \
static bool dummy##name = HaControl::registerIntegrationFactory(&name);

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

