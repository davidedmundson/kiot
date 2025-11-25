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
QList<QFunctionPointer> HaControl::s_integrations;

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

    // create all the integrations
    //TODO read from config for enable or not
    for (auto factory : s_integrations) {
        factory();
    }

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

bool HaControl::registerIntegrationFactory(QFunctionPointer plugin)
{
    HaControl::s_integrations.append(plugin);
    return true;
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
    HaControl::mqttClient()->publish(baseTopic(), "off", 0, false);
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

    m_subscription.reset(); // QtMqtt is either dumb or I'm using it wrong. It seems we need to rebuild a subscription after reconnect, but if you don't delete the old subscription first it shares it.. which does nothing
    m_subscription.reset(HaControl::mqttClient()->subscribe(baseTopic()));
    connect(m_subscription.data(), &QMqttSubscription::messageReceived, this, &Button::triggered);
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

    m_subscription.reset();
    m_subscription.reset(HaControl::mqttClient()->subscribe(baseTopic() + "/set"));
    connect(m_subscription.data(), &QMqttSubscription::messageReceived, this, [this](QMqttMessage message) {
        if (message.payload() == "true") {
            Q_EMIT stateChangeRequested(true);
        } else if (message.payload() == "false") {
            Q_EMIT stateChangeRequested(false);
        } else {
            qWarning() << "unknown state request" << message.payload();
        }
    });
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
//TODO figure this out is this really number or should it be NumberSlider? since its a slider you get?
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

    m_subscription.reset(HaControl::mqttClient()->subscribe(baseTopic() + "/set"));
    connect(m_subscription.data(), &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
        bool ok = false;
        int newValue = message.payload().toInt(&ok);
        if (ok) {
            Q_EMIT valueChangeRequested(newValue);
        } else {
            qWarning() << "Invalid payload for number entity:" << message.payload();
            //TODO figure out if this is a okay way to tell ha what its value should be if it tried to publish a non number value
            setValue(m_value);
        }
    });
}

void Number::setValue(int value)
{
    m_value = value;
    if (HaControl::mqttClient()->state() == QMqttClient::Connected) {
        HaControl::mqttClient()->publish(baseTopic(), QByteArray::number(value), 0, true);
    }
}

#include "core.moc"