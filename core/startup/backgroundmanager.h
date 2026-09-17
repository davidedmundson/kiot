#pragma once

#include <QObject>
#include <QString>
#include "backgroundportal.h" 

class BackgroundManager : public QObject {
    Q_OBJECT
public:
    explicit BackgroundManager(QObject *parent = nullptr);


    bool setupAutostart(bool enabled);
    bool isAutostartEnabled();


private:
    bool enableAutostartup();
    bool disableAutostartup();
    OrgFreedesktopPortalBackgroundInterface *m_backgroundIface;
};
