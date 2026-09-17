// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "ui_qt/mainwindow.h"
#include "core.h"
#include "core/startup/startupmanager.h"
#include "Shared/entities/entities.h"
#include <KConfigGroup>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttClient>
#include <QTimer>
#include <QLoggingCategory>
#include <QApplication>
DEFINE_LOGGER(core, Core.HaControl)

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

void HaControl::validateStartup(bool autostart)
{
    auto startupManager = new StartupManager(this);
    bool currentlyEnabled = startupManager->isAutostartEnabled();

    // Hvis ønsket tilstand matcher det som allerede er satt, trenger vi ikke gjøre noe
    if (currentlyEnabled == autostart) {
        qCDebug(core) << "Autostart is already in desired state:" << autostart;
        return;
    }

    QString actionStr = autostart ? "Enabling" : "Disabling";
    qCInfo(core) << actionStr << " autostartup";

    if (startupManager->setAutostart(autostart)) {
        if (autostart) {
            qCInfo(core) << "Autostart successfully enabled.";
            
            // Hvis vi bruker systemd (og ikke er i Flatpak), kan vi avslutte 
            // slik at systemd tar over kjøringen i bakgrunnen som planlagt.
            if (!PlatformHelper::isFlatpak()) {
                qCInfo(core) << "Running natively with systemd, closing instance to let systemd manage lifecycle.";
                QApplication::exit(0);
            }
        } else {
            qCInfo(core) << "Autostart successfully disabled.";
        }
    } else {
        qCWarning(core) << "Failed to" << (autostart ? "enable" : "disable") << "autostartup";
    }
}

bool HaControl::validateConfig()
{
    auto config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    if (!config->hasGroup("general")) {
        qCWarning(core) << "Config not found, creating default config and launching UI";
        KConfigGroup group(config, "general");
        group.writeEntry("host", "localhost");
        group.writeEntry("port", 1883);
        group.writeEntry("user", "mqtt");
        group.writeEntry("password", "mqtt-password-here");
        group.writeEntry("autostart", false);
        group.writeEntry("useSSL",false);
        group.writeEntry("discoveryprefix","homeassistant");
        config->sync();
        return false;
    }else{
        KConfigGroup group(config, "general");
        if(group.readEntry("password") == "mqtt-password-here"){
            m_mainWindow->show();
        }

    }
    return true;
}
HaControl::HaControl()
{
    s_self = this;
    m_mainWindow = MainWindow::instance();
    if(!validateConfig())
    {
    QProcess::startDetached(QStringLiteral(PROJECT_NAME) );
    QApplication::quit();

    }
    
    auto config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    auto group = config->group("general");
    auto autostart = group.readEntry("autostart", false);
    validateStartup(autostart);
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

    m_connectedNode = new ConnectedNode(this);

    loadIntegrations(config);
    QTimer *reconnectTimer = new QTimer(this);
    reconnectTimer->setInterval(1000);

    connect(reconnectTimer, &QTimer::timeout, this, &HaControl::doConnect);
    //
    // connect(&m_networkConfigurationManager, &QNetworkConfigurationManager::configurationChanged, this, connectToHost);
    //

    connect(m_client, &QMqttClient::stateChanged, this, [reconnectTimer, this](QMqttClient::ClientState state) {
        if (m_mainWindow)
            m_mainWindow->updateIcon(state);
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
    if (m_connectedNode) {
        delete m_connectedNode;
        m_connectedNode = nullptr;
    }
}

void HaControl::doConnect()
{
    auto config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig);
    auto group = config->group("general");
    if (group.readEntry("tls", false)) {
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
                                    {"sw_version", QStringLiteral(PROJECT_VERSION)},
                                    {"manufacturer", QStringLiteral(PROJECT_DEVELOPERS)}, //TODO update to KDE if we manage to make it part of the official portfolio
                                    {"model", QStringLiteral(PROJECT_NAME) },
                                    {"hw_version",QSysInfo::prettyProductName() + " - " + QSysInfo::kernelVersion()}}));

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
