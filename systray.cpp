#include "core.h"

#include <QApplication>
#include <QPainter>
#include <QTimer>
#include <QProcess>
#include <QStandardPaths>
#include <QFile>


#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(st)
Q_LOGGING_CATEGORY(st, "kiot.UI.SystemTray")
SystemTray::SystemTray(QObject *parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    createIcons();

    updateIcon(QMqttClient::ClientState::Disconnected);
    m_trayIcon->setToolTip("Kiot - Disconnected");

    setupMenu();

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &SystemTray::onTrayActivated);

    auto mqttClient = HaControl::mqttClient();
    if (mqttClient) {
        connect(mqttClient, &QMqttClient::stateChanged,
                this, &SystemTray::onMqttStateChanged);
        onMqttStateChanged(mqttClient->state());
    }

    m_trayIcon->show();
    qCDebug(st) << "System tray icon initialized";
}

void SystemTray::onMqttStateChanged(QMqttClient::ClientState state)
{
    const bool connected = (state == QMqttClient::Connected);
    updateIcon(state);

    const QString statusText = connected ? "Connected" : "Disconnected";
    m_trayIcon->setToolTip("Kiot - " + statusText);

}

void SystemTray::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::Trigger:
            openSettings();
            break;
        default:
            break;
    }
}

void SystemTray::onOpenSettings()
{
    openSettings();
}
void SystemTray::onOpenConfig()
{
    openConfig();
}

/**
 * @brief Slot called when "Reconnect" is clicked
 * 
 * @details
 * Attempts to reconnect to MQTT broker.
 */
void SystemTray::onReconnect()
{
    qCDebug(st) << "Manual reconnect requested";

    auto mqttClient = HaControl::mqttClient();
    if (!mqttClient) {
        return;
    }

    if (mqttClient->state() == QMqttClient::Disconnected) {
        mqttClient->connectToHost();
    } else if (mqttClient->state() == QMqttClient::Connected) {
        mqttClient->disconnectFromHost();
        QTimer::singleShot(3000, this, [mqttClient]() {
            mqttClient->connectToHost();
        });
    }
}

/**
 * @brief Closes down kiot
 */
void SystemTray::onQuit()
{
    qCDebug(st) << "Quit requested from system tray";
    QApplication::quit();
}


void SystemTray::createIcons()
{
    QPixmap connectedPixmap(32, 32);
    connectedPixmap.fill(Qt::transparent);
    {
        QPainter p(&connectedPixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(76, 175, 80));
        p.setPen(Qt::NoPen);
        p.drawEllipse(4, 4, 24, 24);
    }
    m_connectedIcon = QIcon(connectedPixmap);

    QPixmap disconnectedPixmap(32, 32);
    disconnectedPixmap.fill(Qt::transparent);
    {
        QPainter p(&disconnectedPixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(244, 67, 54));
        p.setPen(Qt::NoPen);
        p.drawEllipse(4, 4, 24, 24);
    }
    m_disconnectedIcon = QIcon(disconnectedPixmap);

    QPixmap connectingPixmap(32, 32);
    connectingPixmap.fill(Qt::transparent);
    {
        QPainter p(&connectingPixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(255, 193, 7));
        p.setPen(Qt::NoPen);
        p.drawEllipse(4, 4, 24, 24);
    }
    m_connectingIcon = QIcon(connectingPixmap);
}


void SystemTray::setupMenu()
{
    m_statusAction = m_menu->addAction("Status: Disconnected");
    m_statusAction->setEnabled(false);

    m_menu->addSeparator();

    QAction *settingsAction =
        m_menu->addAction(QIcon::fromTheme("configure"), "Open Settings");
    connect(settingsAction, &QAction::triggered,
            this, &SystemTray::onOpenSettings);

    QAction *configAction =
        m_menu->addAction(QIcon::fromTheme("configure"), "Open Config file");
    connect(configAction, &QAction::triggered,
            this, &SystemTray::onOpenConfig);


    QAction *reconnectAction =
        m_menu->addAction(QIcon::fromTheme("view-refresh"), "Reconnect");
    connect(reconnectAction, &QAction::triggered,
            this, &SystemTray::onReconnect);
    m_menu->addSeparator();
    QAction *quitAction =
        m_menu->addAction(QIcon::fromTheme("application-exit"), "Quit");
    connect(quitAction, &QAction::triggered,
            this, &SystemTray::onQuit);

    m_trayIcon->setContextMenu(m_menu);
}


void SystemTray::updateIcon(QMqttClient::ClientState state)
{
    if (state == QMqttClient::Connected) {
        m_trayIcon->setIcon(m_connectedIcon);
        if (m_statusAction) {
            m_statusAction->setText("Status: Connected");
        }
    } else if (state == QMqttClient::Connecting) {
        m_trayIcon->setIcon(m_connectingIcon);
        if (m_statusAction) {
            m_statusAction->setText("Status: Connecting");
        }
    } else {
        m_trayIcon->setIcon(m_disconnectedIcon);
        if (m_statusAction) {
            m_statusAction->setText("Status: Disconnected");
        }
    }
}


void SystemTray::openSettings()
{
    qCDebug(st) << "Opening settings";

    if (QProcess::startDetached("kcmshell6", {"kcm_kiot"})) {
        return;
    }

    const QString configPath =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + "/kiotrc";

    if (QFile::exists(configPath)) {
        QProcess::startDetached("xdg-open", {configPath});
        qCDebug(st) << "Opened config file:" << configPath;
    } else {
        qCWarning(st) << "Could not open settings";
    }
}


void SystemTray::openConfig()
{
    qCDebug(st) << "Opening config file";

    const QString configPath =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + "/kiotrc";

    if (QFile::exists(configPath)) {
        QProcess::startDetached("xdg-open", {configPath});
        qCDebug(st) << "Opened config file:" << configPath;
    } else {
        qCWarning(st) << "Could not open config file";
    }
}


