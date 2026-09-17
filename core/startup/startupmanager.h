#pragma once

#include "backgroundmanager.h"
#include <QObject>
#include <QString>

class SystemdManager;
class DesktopManager;
class BackgroundManager;

class StartupManager : public QObject {
    Q_OBJECT
public:
    explicit StartupManager(QObject *parent = nullptr);
    ~StartupManager();

    bool isAutostartEnabled();
    bool setAutostart(bool enabled);
    bool shouldUseSystemd() const;

private:
    SystemdManager *m_systemdManager;
    DesktopManager *m_desktopManager;
    BackgroundManager *m_backgroundManager;

};