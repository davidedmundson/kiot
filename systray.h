#pragma once

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

#include <QMqttClient>

class SystemTray : public QObject
{
    Q_OBJECT

public:
    explicit SystemTray(QObject *parent = nullptr);
    /**
     * @brief Updates the tray icon based on connection state
     * @param state The MQTT Client state
     */
    void updateIcon(QMqttClient::ClientState state);

private slots:
    void onMqttStateChanged(QMqttClient::ClientState state);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onOpenSettings();
    void onOpenConfig();
    void onReconnect();

    void onQuit();

private:
    /**
     * @brief Creates icons for different connection states
     */
    void createIcons();

    /**
     * @brief Sets up the context menu
     */
    void setupMenu();

    /**
     * @brief Opens Kiot settings with fallback to configuration file
     * @details Tries to open the settings with kcmshell6, if that fails it will try to open the configuration file directly
     *
     * @note This is a fallback for when the settings dialog is not available,
     *
     */
    void openSettings();

    /**
     * @brief Opens Kiot config file
     * @details Tries to open the configuration file directly
     *
     */
    void openConfig();

private:
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_statusAction = nullptr;

    QIcon m_connectedIcon;
    QIcon m_disconnectedIcon;
    QIcon m_connectingIcon;
};
