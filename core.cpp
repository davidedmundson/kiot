// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/entity.h"
#include <KConfigGroup>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttClient>
#include <QTimer>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(core)
Q_LOGGING_CATEGORY(core, "kiot.HaControl")

HaControl *HaControl::s_self = nullptr;
QList<IntegrationFactory> HaControl::s_integrations;

// core internal sensor
class ConnectedNode : public Entity
{
    Q_OBJECT
public:
    ConnectedNode(QObject *parent);
    ~ConnectedNode();
    void init() override;
};

HaControl::HaControl()
{
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
        qCCritical(core) << "Server is not configured, please check " << config->name() << "is configured";
        qCCritical(core) << "kiotrc expected at " << QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    }

    // Loads systray and makes sure its part of config file if missing
    if(!group.hasKey("systray"))
    {
        qCDebug(core) << "systray not configured, defaulting to true";
        group.writeEntry("systray", true); // default to true if not configured
        config->sync();
    }
    if(group.readEntry("systray", true))
        m_systemTray = new SystemTray(this);

    m_connectedNode = new ConnectedNode(this);

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
            qCInfo(core) << "connected";
            break;
        case QMqttClient::Connecting:
            qCInfo(core) << "connecting";
            break;
        case QMqttClient::Disconnected:
            qCWarning(core) << m_client->error();
            qCInfo(core) << "disconnected";
            reconnectTimer->start();
            // do I need to reconnect?
            break;
        }
    });

    doConnect();
}

HaControl::~HaControl()
{
    delete m_connectedNode;
}

void HaControl::doConnect()
{
    auto config = KSharedConfig::openConfig();
    auto group = config->group("general");
    if (group.readEntry("useSSL", false)) {
        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        m_client->connectToHostEncrypted(sslConfig);
    } else {
        m_client->connectToHost();
    }
}

bool HaControl::registerIntegrationFactory(const QString &name, std::function<void()> plugin, bool onByDefault)
{
    s_integrations.append({name, plugin, onByDefault});
    return true;
}

// Kjør integrasjoner
void HaControl::loadIntegrations(KSharedConfigPtr config)
{
    auto integrationconfig = config->group("Integrations");

    if (!integrationconfig.exists()) {
        qCWarning(core) << "Integration group not found in config, defaulting to onByDefault values";
    }

    for (const auto &entry : s_integrations) {
        // Bruk onByDefault hvis config ikke finnes
        if (!integrationconfig.hasKey(entry.name)) {
            integrationconfig.writeEntry(entry.name, entry.onByDefault);
            config->sync();
        }
        bool enabled = integrationconfig.readEntry(entry.name, entry.onByDefault);

        if (enabled) {
            entry.factory();
            qCInfo(core) << "Started integration:" << entry.name;
        } else {
            qCDebug(core) << "Skipped integration:" << entry.name;
        }
    }
}

ConnectedNode::ConnectedNode(QObject *parent)
    : Entity(parent)
{
    setId("connected");
    setName("Connected");
    setHaType("binary_sensor");
    setDiscoveryConfig("state_topic", baseTopic());
    setDiscoveryConfig("payload_on", "on");
    setDiscoveryConfig("payload_off", "off");
    setDiscoveryConfig("device_class", "power");
    setDiscoveryConfig("device",
                       QVariantMap({{"name", hostname()},
                                    {"identifiers", "linux_ha_bridge_" + hostname()},
                                    {"sw_version", QStringLiteral(KIOT_VERSION)},
                                    {"manufacturer", "Linux HA Bridge"},
                                    {"model", "Linux"}}));

    auto c = HaControl::mqttClient();
    c->setWillTopic(baseTopic());
    c->setWillMessage("off");
    c->setWillRetain(true);
}

ConnectedNode::~ConnectedNode()
{
    HaControl::mqttClient()->publish(baseTopic(), "off", 0, true);
}

void ConnectedNode::init()
{
    sendRegistration();
    HaControl::mqttClient()->publish(baseTopic(), "on", 0, true);
}

#include "core.moc"
