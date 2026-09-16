#pragma once

#include <QObject>
#include <QString>

class SystemdManager;
class DesktopManager;

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

};