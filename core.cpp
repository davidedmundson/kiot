// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/entity.h"
#include <KConfigGroup>
#include <KNotification>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttClient>
#include <QProcess>
#include <QTimer>
#include <KSandbox>
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
    // Checks that config is valid and opens the kcm module if its not
    if (!ensureConfigDefaults(config)) {
        KNotification::event(KNotification::Notification,
                             QString("Invalid Config"),
                             QString("Config file is not valid, please fill out everything in the general tab"));
        QProcess::startDetached("kcmshell6", {"kcm_kiot"});
        
        // This timer was added to give the notification time to show 
        // before we close down and direct users at what needs to be done
        QTimer::singleShot(5000, this, [this]() {
            qCCritical(core) << "Config file is invalid please fill it correctly";
            QApplication::quit();
        });
    }
    // Setup the service manager and makes sure our user service is correct
    m_serviceManager = new ServiceManager(this);
    validateStartup();

    // Setup the mqtt client
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

    // Loads systray icon if enabled
    if (group.readEntry("systray", true))
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
        if (m_systemTray)
            m_systemTray->updateIcon(state);
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
// To make sure the user based service is installed if enabled and removed if not
void HaControl::validateStartup() {
    // Les config
    auto config = KSharedConfig::openConfig("kiotrc", KSharedConfig::CascadeConfig);
    KConfigGroup generalGroup(config, "general");
    bool autostartEnabled = generalGroup.readEntry("autostart", false);
    
    // Sjekk om service er riktig konfigurert
    bool serviceEnabled = m_serviceManager->isAutostartEnabled();
    
    if (autostartEnabled != serviceEnabled) {
        qCInfo(core) << "Autostart config mismatch. Config:" << autostartEnabled 
                     << "Service:" << serviceEnabled << "- Syncing...";
        
        m_serviceManager->setupAutostart(autostartEnabled);
    }
}
// A function to check and validate needed config values
bool HaControl::ensureConfigDefaults(KSharedConfigPtr config)
{
    bool configValid = true;
    bool configChanged = false;

    // Sjekk og initialiser "general" gruppen
    KConfigGroup generalGroup = config->group("general");

    // Liste over nødvendige felter i general gruppen
    const QVector<QPair<QString, QVariant>> generalDefaults = {
        {"host", ""}, // MQTT server hostname/IP
        {"port", 1883}, // MQTT port
        {"user", ""}, // MQTT username
        {"password", ""}, // MQTT password
        {"useSSL", false}, // SSL/TLS
        {"systray", true}, // System tray icon
        {"autostart", true} // Autostart
    };

    // Sjekk om general gruppen eksisterer og har alle nødvendige nøkler
    if (!generalGroup.exists()) {
        qCWarning(core) << "General configuration group not found, creating with defaults";
        configValid = false;
    }

    // Legg til manglende nøkler med standardverdier
    for (const auto &[key, defaultValue] : generalDefaults) {
        if (!generalGroup.hasKey(key)) {
            generalGroup.writeEntry(key, defaultValue);
            configChanged = true;
            qCDebug(core) << "Added missing config key:" << key << "with default value:" << defaultValue;

            // Mark config as invalid if a critical field is missing
            if (key == "host" && defaultValue.toString().isEmpty()) {
                configValid = false;
            }
        } else if (generalGroup.hasKey(key)) {
            auto readValue = generalGroup.readEntry(key, "");
            if (readValue.isEmpty() || readValue == "") {
                if (key == "host" || key == "port" || key == "user" || key == "password")
                    configValid = false;
            }
        }
    }

    if (configChanged) {
        config->sync();
        qCDebug(core) << "Configuration updated with default values";
    }

    if (generalGroup.readEntry("host", "").isEmpty()) {
        qCCritical(core) << "MQTT host is not configured!";
        configValid = false;
    }

    return configValid;
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

// Start the integrations
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

// ConnectedNode start

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
    setDiscoveryConfig("entity_category","diagnostic");
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
