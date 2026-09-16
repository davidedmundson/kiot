// SPDX-FileCopyrightText: 2025
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "core/core.h"
#include "mainwindow.h"
#include "settingsmanager.h"

#include <QApplication>
#include <QCloseEvent>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QStandardPaths>
#include <QProcess>
#include <QPainter>
#include <QQmlContext>
#include <QQmlEngine>


DEFINE_LOGGER(mw,  UI.Main)

MainWindow *MainWindow::s_instance = nullptr;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_trayIcon(nullptr)
    , m_quickWidget(nullptr)
    , m_menu(new QMenu())
{
    s_instance = this;
    
    
    setWindowTitle( QString(PROJECT_NAME) + " Settings");
    setWindowIcon(QIcon::fromTheme(PlatformHelper::generateServiceName()));
    setMinimumSize(800, 600);
    
    // Create central widget for QML
    m_quickWidget = new QQuickWidget(this);
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    setCentralWidget(m_quickWidget);

    // Setup QML
    setupQml();       
    
    // Setup system tray
    setupSystemTray();

    
    // Load window geometry
    loadWindowState();
    
    
    // Start hidden
    hide();
    
    
    qCDebug(mw) << "UI initialized";
}
MainWindow::~MainWindow()
{
    saveWindowState();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

WId MainWindow::getWindowId()
{

    QWindow *window = this->windowHandle(); // hent QWindow fra QMainWindow
    if (!window) {
        this->createWinId(); // sørg for at vinduet får en native handle
        window = this->windowHandle();
        if (!window) {
            return 0; // fortsatt ikke klar
        }
    }

    return window->winId(); // native window ID
}

void MainWindow::setupQml()
{
    // Setup engine and context
    QQmlEngine *engine = m_quickWidget->engine();
    QQmlContext *context = engine->rootContext();
    
    // Create and add settings manager context property
    SettingsManager *settingsManager = new SettingsManager(this);
    context->setContextProperty("settingsManager", settingsManager);
    
    // Load QML from resources
    m_quickWidget->setSource(QUrl("qrc:/imports/main_qt/window/core/ui_qt/ui/main.qml"));
}

//============ Start of the slots of mainwindow ========================/

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        toggleVisibility();
        break;
    default:
        break;
    }
}

void MainWindow::onMqttStateChanged(QMqttClient::ClientState state)
{
    const bool connected = (state == QMqttClient::Connected);
    updateIcon(state);

    const QString statusText = connected ? "Connected" : "Disconnected";
    m_trayIcon->setToolTip(QStringLiteral(PROJECT_NAME) + " " + statusText);
}
void MainWindow::onOpenSettings()
{
    toggleVisibility();
}
void MainWindow::onOpenConfig()
{
    QString confFile = PlatformHelper::configFilePath();
    QStringList args = QProcess::splitCommand("xdg-open " + confFile);
    QProcess process;
    process.setProgram(args.takeFirst());
    process.setArguments(args);
    PlatformHelper::startHostProcess(process);
    process.deleteLater();
    //TODO open filepath
}

/**
 * @brief Slot called when "Reconnect" is clicked
 *
 * @details
 * Attempts to reconnect to MQTT broker.
 */
void MainWindow::onReconnect()
{
    qCDebug(mw) << "Manual reconnect requested";

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
void MainWindow::onQuit()
{
    qCDebug(mw) << "Quit requested from system tray";
    QApplication::quit();
}
/**
 * @brief Restarts kiot
 */
void MainWindow::onRestart()
{
    qCDebug(mw) << "Restart requested from system tray";
    QProcess::startDetached(QStringLiteral(PROJECT_NAME) );
    QApplication::quit();
}


void MainWindow::toggleVisibility()
{
    if (isVisible()) {
        hide();
    } else {
        show();
        raise();
        activateWindow();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Don't quit on close, just hide to tray
    hide();
    event->ignore();
}

void MainWindow::loadWindowState()
{
    QSettings settings(QApplication::organizationName(), QApplication::applicationName());
    
    restoreGeometry(settings.value("windowGeometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::saveWindowState()
{
    QSettings settings(QApplication::organizationName(), QApplication::applicationName());
    
    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

/**
 * @brief Systray icon spesific code from here
 *
 */
void MainWindow::setupSystemTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qCWarning(mw) << "System tray not available";
        return;
    }
    if(m_trayIcon ==  nullptr)
        m_trayIcon = new QSystemTrayIcon(this);
    createIcons();
    m_statusAction = m_menu->addAction("Status: Disconnected");
    m_statusAction->setEnabled(false);
    m_versionAction = m_menu->addAction("Version: " +  QStringLiteral(PROJECT_VERSION));
    m_versionAction->setEnabled(false);
    m_menu->addSeparator();
    QAction *settingsAction = m_menu->addAction(QIcon::fromTheme("configure"), "Open Settings");
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onOpenSettings);

    QAction *configAction = m_menu->addAction(QIcon::fromTheme("configure"), "Open Config file");
    connect(configAction, &QAction::triggered, this, &MainWindow::onOpenConfig);

    QAction *reconnectAction = m_menu->addAction(QIcon::fromTheme("view-refresh"), "Reconnect");
    connect(reconnectAction, &QAction::triggered, this, &MainWindow::onReconnect);
    m_menu->addSeparator();

    QAction *restartAction = m_menu->addAction(QIcon::fromTheme("system-reboot"), "Restart");
    connect(restartAction, &QAction::triggered, this, &MainWindow::onRestart);

    QAction *quitAction = m_menu->addAction(QIcon::fromTheme("application-exit"), "Quit");
    connect(quitAction, &QAction::triggered, this, &MainWindow::onQuit);

    m_trayIcon->setContextMenu(m_menu);
    updateIcon(QMqttClient::ClientState::Disconnected);
    // Connect tray icon activation
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    
    m_trayIcon->show();
    qCDebug(mw) << "System tray initialized";
}

void MainWindow::createIcons()
{
    // Connected icon (green)
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

    // Disconnected icon (red)
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

    // Connecting icon (yellow)
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


void MainWindow::updateIcon(QMqttClient::ClientState state)
{
    if(!m_trayIcon)
       return;
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
    const bool connected = (state == QMqttClient::Connected);
    const QString statusText = connected ? "Connected" : "Disconnected";
    m_trayIcon->setToolTip(QStringLiteral(PROJECT_NAME) + " - " + statusText);
}

void MainWindow::sendNotification(const QString &title, const QString &msg, QSystemTrayIcon::MessageIcon icon, int millisecondsTimeoutHint)
{
    if (!s_instance || !s_instance->m_trayIcon) {
        qCWarning(mw) << "Cannot send notification: SystemTray not initialized";
        return;
    }
    s_instance->m_trayIcon->showMessage(title, msg, icon, millisecondsTimeoutHint);
}
