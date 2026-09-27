// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "ui_qt/mainwindow.h"
#include "core.h"
#include "core/startup/startupmanager.h"
#include "plugins/pluginmanager.h"
#include <KIOTShared/kiotshared.h>

#include <KConfigGroup>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttClient>
#include <QTimer>
#include <QLoggingCategory>
#include <QApplication>

using KIOTShared::Entities::Entity;
using KIOTShared::Transport::TransportManager;
using KIOTShared::PlatformHelper;
using KIOTShared::Config::ConfigManager;
DEFINE_LOGGER(core, Core.HaControl)

HaControl *HaControl::s_self = nullptr;


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
    if (currentlyEnabled == autostart) {
        qCDebug(core) << "Autostart is already in desired state:" << autostart;
        return;
    }

    QString actionStr = autostart ? "Enabling" : "Disabling";
    qCInfo(core) << actionStr << " autostartup";

    if (startupManager->setAutostart(autostart)) {
        if (autostart) {
            qCInfo(core) << "Autostart successfully enabled.";
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
            MainWindow::sendNotification(QString(PROJECT_NAME),"Please configure your MQTT settings");
            m_mainWindow->show();
        }

    }
    return true;
}
HaControl::HaControl()
{
    m_pluginManager = new PluginManager(this);
    s_self = this;
    m_mainWindow = MainWindow::instance();
    ConfigManager *m_conf = new ConfigManager(this);
    qCDebug(core) << m_conf->filePath();
    if(!validateConfig()) {
        QProcess::startDetached(QStringLiteral(PROJECT_NAME));
        QApplication::quit();
        return;
    }
    
    auto config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig);
    auto group = config->group("general");
    auto autostart = group.readEntry("autostart", false);
    validateStartup(autostart);

    auto transportManager = new TransportManager(this);

    if (m_mainWindow) {
        connect(transportManager, &TransportManager::connectionStateChanged, 
                m_mainWindow, &MainWindow::updateIcon);
    }
    
    connect(transportManager, &TransportManager::mqttConfigMissing, this, [this]() {
        if (m_mainWindow) {
            MainWindow::sendNotification(QString(PROJECT_NAME), "Please configure your MQTT settings");
            m_mainWindow->show();
        }
    });

    m_connectedNode = new ConnectedNode(this);

    loadIntegrations();

    transportManager->doConnect();
}

HaControl::~HaControl()
{
    if (m_connectedNode) {
        delete m_connectedNode;
        m_connectedNode = nullptr;
    }
    if(m_pluginManager)
    {
        delete m_pluginManager;
        m_pluginManager = nullptr;
    }

}


void HaControl::loadIntegrations()
{
    if(m_pluginManager)
        m_pluginManager->loadActivatedPlugins();
    else
        qCWarning(core) << "PluginManager not initialized";
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

    auto c = TransportManager::mqttClient();
    c->setWillTopic(baseTopic());
    c->setWillMessage("off");
    c->setWillRetain(true);
}

ConnectedNode::~ConnectedNode()
{
    auto client = TransportManager::mqttClient();
    if(client)
        client->publish(baseTopic(), "off", 0, true);
}

void ConnectedNode::init()
{
    sendRegistration();
    TransportManager::mqttClient()->publish(baseTopic(), "on", 0, true);
}

#include "core.moc"
