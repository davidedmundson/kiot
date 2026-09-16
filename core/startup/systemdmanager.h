#pragma once

#include <QObject>
#include <QString>

class SystemdManager : public QObject {
    Q_OBJECT
public:
    explicit SystemdManager(QObject *parent = nullptr);


    bool setupAutostart(bool enabled);
    bool isAutostartEnabled();


    static QString serviceFilePath();
    static QString serviceContent();


private:

    bool writeServiceFile();
    bool removeServiceFile();
    bool enableServiceViaDBus();
    bool disableServiceViaDBus();
};
