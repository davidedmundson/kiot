#pragma once

#include <QObject>
#include <QString>

class ServiceManager : public QObject {
    Q_OBJECT
public:
    explicit ServiceManager(QObject *parent = nullptr);
    
    bool setupAutostart(bool enabled);
    bool isAutostartEnabled();
    
    static QString serviceFilePath();
    static QString serviceContent();
    
private:
    bool writeServiceFile();
    bool removeServiceFile();
    bool enableServiceViaDBus();
    bool disableServiceViaDBus();
    bool isFlatpak();
};