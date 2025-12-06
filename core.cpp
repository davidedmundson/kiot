// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"

#include <QDebug>
#include <QMqttClient>
#include <QtGlobal>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <KConfig>
#include <KSharedConfig>
#include <KConfigGroup>

HaControl *HaControl::s_self = nullptr;
QList<IntegrationFactory> HaControl::s_integrations;

// core internal sensor
class ConnectedNode: public Entity
{
    Q_OBJECT
public:
    ConnectedNode(QObject *parent);
    ~ConnectedNode();
};

HaControl::HaControl() {
    s_self = this;
    auto config = KSharedConfig::openConfig();
    auto group = config->group("general");
    m_client = new QMqttClient(this);
    m_client->setHostname(group.readEntry("host"));
    m_client->setPort(group.readEntry("port", 1883));
    m_client->setUsername(group.readEntry("user"));
    m_client->setPassword(group.readEntry("password"));
    m_client->setKeepAlive(3); // set a low ping so we become unavailable on suspend quickly

    if (m_client->hostname().isEmpty()) {
        qCritical() << "Server is not configured, please check " << config->name() << "is configured";
        qCritical() << "kiotrc expected at " << QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    }

    new ConnectedNode(this);
    
    loadIntegrations(config);
    
    QTimer *reconnectTimer = new QTimer(this);
    reconnectTimer->setInterval(1000);

    connect(reconnectTimer, &QTimer::timeout, this, &HaControl::doConnect);
           //
           // connect(&m_networkConfigurationManager, &QNetworkConfigurationManager::configurationChanged, this, connectToHost);
           //

    connect(m_client, &QMqttClient::stateChanged, this, [reconnectTimer, this](QMqttClient::ClientState state) {

        switch (state) {
        case QMqttClient::Connected:
            qDebug() << "connected";
            break;
        case QMqttClient::Connecting:
            qDebug() << "connecting";
            break;
        case QMqttClient::Disconnected:
            qDebug() << m_client->error();
            qDebug() << "disconnected";
            reconnectTimer->start();
            //do I need to reconnect?
            break;
        }
    });

    doConnect();

}

HaControl::~HaControl()
{
}

void HaControl::doConnect()
{
    auto config = KSharedConfig::openConfig();
    auto group = config->group("general");
    if (group.readEntry("useSSL", false)) {
        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        m_client ->connectToHostEncrypted(sslConfig);
    } else {
        m_client->connectToHost();
    }
}

bool HaControl::registerIntegrationFactory(const QString &name, std::function<void()> plugin, bool onByDefault)
{
    s_integrations.append({name, plugin, onByDefault});
    return true;
}

// load integrations from config, if not found, use onByDefault from the integration fac
void HaControl::loadIntegrations(KSharedConfigPtr config)
{
    
    auto integrationconfig = config->group("Integrations");

    if(!integrationconfig.exists()){
        qWarning() << "Integration group not found in config, defaulting to onByDefault values";
    }

    for (const auto &entry : s_integrations) {
       // if the key doesn't exist, write it to the config
        if(!integrationconfig.hasKey(entry.name)) {
            integrationconfig.writeEntry(entry.name, entry.onByDefault);
            config->sync();
        }
        bool enabled = integrationconfig.readEntry(entry.name, entry.onByDefault);

        if(enabled){
            entry.factory();
            qDebug() << "Started integration:" << entry.name;
        } else {
            qDebug() << "Skipped integration:" << entry.name;
        }
    }
}

static QString s_discoveryPrefix = "homeassistant";

Entity::Entity(QObject *parent):
    QObject(parent)
{
    connect(HaControl::mqttClient(), &QMqttClient::connected, this, &Entity::init);
}

QString Entity::hostname() const
{
    return QHostInfo::localHostName().toLower();
}

QString Entity::baseTopic() const
{
    return hostname() + "/" + id();
}

void Entity::setHaConfig(const QVariantMap &newHaConfig)
{
    m_haConfig = newHaConfig;
}

QString Entity::haType() const
{
    return m_haType;
}

void Entity::setHaType(const QString &newHaType)
{
    m_haType = newHaType;
}

QString Entity::name() const
{
    return m_name;
}

void Entity::setDiscoveryConfig(const QString &key, const QVariant &value)
{
    m_haConfig[key] = value;
}

void Entity::setName(const QString &newName)
{
    m_name = newName;
}

QString Entity::id() const
{
    return m_id;
}

void Entity::setId(const QString &newId)
{
    m_id = newId;
}

void Entity::init()
{}

void Entity::sendRegistration()
{
    if (haType().isEmpty()) {
        return;
    }
    QVariantMap config = m_haConfig;
    config["name"] = name();

    if (id() != "connected") { //special case
        config["availability_topic"] = hostname() + "/connected";
        config["payload_available"] = "on";
        config["payload_not_available"] = "off";
    }
    if (!config.contains("device")) {
        config["device"] = QVariantMap({{"identifiers", "linux_ha_bridge_" + hostname() }});
    }
    config["unique_id"] = "linux_ha_control_"+ hostname() + "_" + id();

    HaControl::mqttClient()->publish(s_discoveryPrefix + "/" + haType() + "/" + hostname() + "/" + id() + "/config", QJsonDocument(QJsonObject::fromVariantMap(config)).toJson(QJsonDocument::Compact), 0, true);
    // TODO learn more about the mqtt home assistant communication to figure out the best way to manage this so
    // we get a available entity the first time its registred by kiot without needing to restart the process
    if (id() != "connected") { //special case
        HaControl::mqttClient()->publish(hostname() + "/connected", "on", 0, false);
    }
}


ConnectedNode::ConnectedNode(QObject *parent):
    Entity(parent)
{
    setId("connected");
    setName("Connected");
    setHaType("binary_sensor");
    setHaConfig({
        {"state_topic", baseTopic()},
        {"payload_on", "on"},
        {"payload_off", "off"},
        {"device_class", "power"},
        {"device", QVariantMap({
                       {"name", hostname() },
                       {"identifiers", "linux_ha_bridge_" + hostname() },
                       {"sw_version", "0.1"},
                       {"manufacturer", "Linux HA Bridge"},
                       {"model", "Linux"}
                   })}
    });

    auto c = HaControl::mqttClient();
    c->setWillTopic(baseTopic());
    c->setWillMessage("off");
    c->setWillRetain(true);

    connect(HaControl::mqttClient(), &QMqttClient::connected, this, [this]() {
        sendRegistration();
        HaControl::mqttClient()->publish(baseTopic(), "on", 0, false);
    });
}

ConnectedNode::~ConnectedNode()
{
    // TODO find a good way to let this entity publish before shutdown is done and not cause a coredump
    //    HaControl::mqttClient()->publish(baseTopic(), "off", 0, false);
    
}

Button::Button(QObject *parent)
    : Entity(parent)
{
}

void Button::init()
{
    setHaType("button");
    setHaConfig({
        {"command_topic", baseTopic()}
    });
    sendRegistration();

    auto subscription = HaControl::mqttClient()->subscribe(baseTopic());
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &){
            Q_EMIT triggered();
        });
    }
}

Switch::Switch(QObject *parent)
    : Entity(parent)
{
    setHaType("switch");
}

void Switch::init()
{
    setHaConfig({
        {"state_topic", baseTopic()},
        {"command_topic", baseTopic() + "/set"},
        {"payload_on", "true"},
        {"payload_off", "false"}
    });

    sendRegistration();
    setState(m_state);

    auto subscription = HaControl::mqttClient()->subscribe(baseTopic() + "/set");
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
            if (message.payload() == "true") {
                Q_EMIT stateChangeRequested(true);
            } else if (message.payload() == "false") {
                Q_EMIT stateChangeRequested(false);
            } else {
                qWarning() << "unknown state request" << message.payload();
            }
        });
    }
}
void Switch::setState(bool state)
{
    m_state = state;
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), state ? "true" : "false", 0, true);
    }
}

BinarySensor::BinarySensor(QObject *parent)
    : Entity(parent)
{
}

void BinarySensor::init()
{
    setHaType("binary_sensor");
    setHaConfig({
        {"state_topic", baseTopic()},
        {"payload_on", "true"},
        {"payload_off", "false"}
    });
    sendRegistration();
    publish();
}

void BinarySensor::publish()
{
    qDebug() << name() << "publishing state" << m_state;
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), m_state ? "true" : "false", 0, true);
    }
}

void BinarySensor::setState(bool state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    publish();
}

bool BinarySensor::state() const
{
    return m_state;
}

Sensor::Sensor(QObject *parent)
    : Entity(parent)
{
}

void Sensor::init()
{
    setHaType("sensor");

    // Standard state topic
    setHaConfig({
        {"state_topic", baseTopic()},
        {"json_attributes_topic", baseTopic() + "/attributes"} // ny topic for attributes
    });

    sendRegistration();
    publishState();
    publishAttributes();
}

void Sensor::setState(const QString &state)
{
    m_state = state;
    publishState();
}

void Sensor::setAttributes(const QVariantMap &attrs)
{
    m_attributes = attrs;
    publishAttributes();
}

void Sensor::publishState()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    HaControl::mqttClient()->publish(baseTopic(), m_state.toUtf8(), 0, true);
}

void Sensor::publishAttributes()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;

    QJsonObject obj;
    for (auto it = m_attributes.constBegin(); it != m_attributes.constEnd(); ++it)
        obj[it.key()] = QJsonValue::fromVariant(it.value());

    QJsonDocument doc(obj);
    HaControl::mqttClient()->publish(baseTopic() + "/attributes", doc.toJson(QJsonDocument::Compact), 0, true);
}


Event::Event(QObject *parent)
    : Entity(parent)
{
}

void Event::init()
{
    setHaType("device_automation");
    setHaConfig({
        {"automation_type", "trigger"},
        {"topic", baseTopic()},
        {"type", {"button_short_press"}},
        {"subtype", name()}
    });
    sendRegistration();
}

void Event::trigger()
{
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), "pressed", 0, false);
        HaControl::mqttClient()->publish(baseTopic(), "", 0, true);
    }
}

Number::Number(QObject *parent)
    : Entity(parent)
{
    setHaType("number");
}

void Number::setRange(int min, int max, int step, const QString &unit)
{
    m_min = min;
    m_max = max;
    m_step = step;
    m_unit = unit;
}

void Number::init()
{
    setHaConfig({ {"state_topic", baseTopic()},
            {"command_topic", baseTopic() + "/set"},
            {"min", QString::number(m_min)},
            {"max", QString::number(m_max)},
            {"step", QString::number(m_step)},
            {"unit_of_measurement", m_unit}
    });
 

    sendRegistration();

    setValue(m_value);

    auto subscription = HaControl::mqttClient()->subscribe(baseTopic() + "/set");
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this,
            [this](const QMqttMessage &message) {
                bool ok = false;
                int newValue = message.payload().toInt(&ok);
                if (ok) {
                    Q_EMIT valueChangeRequested(newValue);
                } else {
                    qWarning() << "Invalid payload for number entity:" << message.payload();
                }
            });
    }
}

void Number::setValue(int value)
{
    m_value = value;
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), QByteArray::number(value), 0, true);
    }
}

int Number::getValue()
{
    return m_value;
}

// ========== Select Entity Implementation ==========

Select::Select(QObject *parent, const QString &initialState, const QStringList &options)
    : Entity(parent), m_state(initialState), m_options(options)
{
    setHaType("select");
    // Validate initial state
    if (m_options.contains(initialState)) {
        m_state = initialState;
    } else if (!m_options.isEmpty()) {
        m_state = m_options.first();
    }
}

void Select::init()
{
    setHaConfig({
        {"state_topic", baseTopic()},
        {"command_topic", baseTopic() + "/set"},
        {"options",m_options}
    });

    sendRegistration();
   
    auto subscription = HaControl::mqttClient()->subscribe(baseTopic() + "/set");
    if (subscription) {
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
            const QString newValue = QString::fromUtf8(message.payload());
            m_state = newValue;
            emit optionSelected(newValue);
        });
    }
}

void Select::setOptions(const QStringList &opts)
{
    m_options = opts;
    //Check that current state is valid with new options
    if (!m_state.isEmpty() && !m_options.contains(m_state)) {
        qWarning() << "Select" << name() << "current state" << m_state << "no longer valid, resetting";
        m_state = m_options.isEmpty() ? QString() : m_options.first();
    }
    
    setHaConfig({
        {"state_topic", baseTopic()},
        {"command_topic", baseTopic() + "/set"},
        {"options",m_options}
    });
    sendRegistration();
    publishState();
}

void Select::setState(const QString &state)
{
    //Tries to make sure you can only set the state to valid options or empty
    if (m_options.isEmpty() && !state.isEmpty()) {
        qWarning() << "Select" << name() << "has no options defined, state changed from " << state << "to" << QString();
        m_state = QString(); 
    }
    else if(!m_options.contains(state) && !state.isEmpty())
    {
        qWarning() << "Select" << name() << " can not be set to state: " << state << " as it is not in the options list" << m_options;
        return;
    }
    else {
        m_state = state;
    }
    publishState();
}



void Select::publishState()
{
    if (HaControl::mqttClient()->state() != QMqttClient::Connected)
        return;   
    HaControl::mqttClient()->publish(baseTopic(), m_state.toUtf8(), 0, true);
}


#include "core.moc"

