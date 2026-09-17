// SPDX-FileCopyrightText: 2025
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "Shared/platformhelper.h"
#include "settingsmanager.h"
#include <QMainWindow>
#include <QQuickWidget>
#include <QWindow>
#include <QWidget>
#include <QSystemTrayIcon>
#include <QMqttClient>
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    static MainWindow *instance() { return s_instance; }

    void updateIcon(QMqttClient::ClientState state);
    static void sendNotification(const QString &title, const QString &msg, 
                                QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, 
                                int millisecondsTimeoutHint = 10000);
    WId getWindowId();
public slots:
    void toggleVisibility();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onMqttStateChanged(QMqttClient::ClientState state);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onOpenSettings();
    void onOpenConfig();
    void onReconnect();
    void onRestart();
    void onQuit();

private:
    void createIcons();

    void setupSystemTray();
    void loadWindowState();
    void saveWindowState();
    void setupQml();

    QSystemTrayIcon *m_trayIcon = nullptr;
    QQuickWidget *m_quickWidget = nullptr;
    
    QIcon m_connectedIcon;
    QIcon m_disconnectedIcon;
    QIcon m_connectingIcon;
    
    QAction *m_statusAction = nullptr;
    QAction *m_versionAction = nullptr;
    QMenu *m_menu = nullptr;

    static MainWindow *s_instance;
};

#endif // MAINWINDOW_H