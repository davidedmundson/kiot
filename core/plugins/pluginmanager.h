#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPluginLoader>
#include <KIOTShared/kiotshared.h>

using KIOTShared::PlatformHelper;
using KIOTShared::Plugins::KIOTPluginInterface;

struct LoadedPluginA {
        KIOTPluginInterface *interface = nullptr;
        QPluginLoader *loader = nullptr;
};


class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager();

    QStringList pluginFiles();
    bool loadActivatedPlugins();
    void unloadAllPlugins();
    bool startPlugin(QString pluginName);
    bool stopPlugin(QString pluginName);
    
private:

    void loadPluginList();
    QStringList m_pluginFiles = {};
    QHash<QString, LoadedPluginA> m_loadedPlugins;
};