#pragma once

#include <QObject>
#include <QString>

class DesktopManager : public QObject {
    Q_OBJECT
public:
    explicit DesktopManager(QObject *parent = nullptr);
    ~DesktopManager() = default;

    bool setupAutostart(bool enabled);
    bool isAutostartEnabled();

private:
    QString desktopFilePath();
    QString desktopFileContent();
    bool writeDesktopFile();
    bool removeDesktopFile();
};