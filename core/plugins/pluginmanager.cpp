#include "pluginmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <KConfigGroup>
#include <KSharedConfig>
DEFINE_LOGGER(pm_logger, Core.Plugins.PluginManager)

PluginManager::PluginManager(QObject *parent): QObject(parent)
{

    loadPluginList(); 

}


PluginManager::~PluginManager()
{
    unloadAllPlugins();
}

void PluginManager::unloadAllPlugins()
{
    for (auto it = m_loadedPlugins.cbegin(); it != m_loadedPlugins.cend(); ++it) {
        const QString &name = it.key();
        const LoadedPluginA &plugin = it.value();

        if (plugin.interface) {
            plugin.interface->stopPlugin();
        }
        if (plugin.loader) {
            plugin.loader->unload();
        }
    }
    m_loadedPlugins.clear();
}

bool PluginManager::startPlugin(QString pluginName)
{
    if (m_loadedPlugins.contains(pluginName)) {
        qCInfo(pm_logger) << "Plugin" << pluginName << "is already loaded";
        return true;
    }
    
    for (QString fileName : m_pluginFiles)
    {
        if(!fileName.endsWith(".so"))
            continue;
        auto pluginLoader = new QPluginLoader(fileName, this);
        QObject *pluginInstance = pluginLoader->instance();
        if (pluginInstance) {
            auto *kiotPlugin = qobject_cast<KIOTShared::Plugins::KIOTPluginInterface *>(pluginInstance);
            if (kiotPlugin) {
                QString pluginNameToLoad = kiotPlugin->name();
                if(pluginNameToLoad == pluginName)
                {
                    if (kiotPlugin->checkCompatibility()) {
                        if (kiotPlugin->startPlugin()) {
                            qCInfo(pm_logger) << "Started plugin integration:" << pluginName;
                            m_loadedPlugins[pluginName] = {kiotPlugin, pluginLoader};
                        } else {
                            qCWarning(pm_logger) << "Failed to start plugin:" << pluginName;
                            pluginLoader->unload();
                            pluginLoader->deleteLater();
                        }
                    } else {
                        qCWarning(pm_logger) << "Plugin compatibility check failed for:" << pluginName << "Disabling";
                        pluginLoader->unload();
                        pluginLoader->deleteLater();
                    }
                }
            }else {
                qCWarning(pm_logger) << "File" << fileName << "does not cast to KIOTPluginInterface!";
                pluginLoader->unload();
                pluginLoader->deleteLater();
            }
        }else {
            qCWarning(pm_logger) << "Failed to load plugin from file" << fileName << ":" << pluginLoader->errorString();
            pluginLoader->deleteLater();
        }
    }
    qCWarning(pm_logger) << "Plugin" << pluginName << "failed to load";
    //TODO implement me
    return false;
}

bool PluginManager::stopPlugin(QString pluginName)
{
    if (!m_loadedPlugins.contains(pluginName)) {
        qCWarning(pm_logger) << "Plugin" << pluginName << "is not loaded";
        return false;
    }
    
    // Hent ut verdien direkte
    LoadedPluginA plugin = m_loadedPlugins.value(pluginName);
    
    if (plugin.interface) {
        plugin.interface->stopPlugin();
    }
    if (plugin.loader) {
        plugin.loader->unload();
    }
    
    // Fjern direkte fra hashen ved hjelp av nøkkelen
    m_loadedPlugins.remove(pluginName);
    qCInfo(pm_logger) << "Stopped plugin" << pluginName;
    return true;
}

bool PluginManager::loadActivatedPlugins()
{
    if(m_pluginFiles.size() == 0)
    {
        loadPluginList();
        if(m_pluginFiles.size() == 0)
        {
            qCWarning(pm_logger) << "No plugins found";
            return false;
        }
    }
    auto config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig);
    auto integrationconfig = config->group("Integrations");
    for (QString fileName : m_pluginFiles)
    {
        if(!fileName.endsWith(".so"))
            continue;

        auto pluginLoader = new QPluginLoader(fileName, this);
        QObject *pluginInstance = pluginLoader->instance();
        if (pluginInstance) {
            auto *kiotPlugin = qobject_cast<KIOTShared::Plugins::KIOTPluginInterface *>(pluginInstance);
            if (kiotPlugin) {
                QString pluginName = kiotPlugin->name();

                if (!integrationconfig.hasKey(pluginName)) {
                    integrationconfig.writeEntry(pluginName, kiotPlugin->enabledByDefault());
                    config->sync();
                }
                bool enabled = integrationconfig.readEntry(pluginName, false);
                if (enabled) {
                    if (kiotPlugin->checkCompatibility()) {
                        if (kiotPlugin->startPlugin()) {
                            qCInfo(pm_logger) << "Started plugin integration:" << pluginName;
                            m_loadedPlugins[pluginName] = {kiotPlugin, pluginLoader};
                           // m_loadedPlugins.append({kiotPlugin, pluginLoader});
                        } else {
                            qCWarning(pm_logger) << "Failed to start plugin:" << pluginName;
                            pluginLoader->unload();
                            pluginLoader->deleteLater();
                        }
                    } else {
                        qCWarning(pm_logger) << "Plugin compatibility check failed for:" << pluginName << "Disabling";
                        integrationconfig.writeEntry(pluginName, false);
                        config->sync();
                        pluginLoader->unload();
                        pluginLoader->deleteLater();
                    }
                } else {
                    qCDebug(pm_logger) << "Skipped disabled plugin:" << pluginName;
                    pluginLoader->unload();
                    pluginLoader->deleteLater();
                }
            }else {
                qCWarning(pm_logger) << "File" << fileName << "does not cast to KIOTPluginInterface!";
                pluginLoader->unload();
                pluginLoader->deleteLater();
            }
        }else {
            qCWarning(pm_logger) << "Failed to load plugin from file" << fileName << ":" << pluginLoader->errorString();
            pluginLoader->deleteLater();
        }
    }
    qCDebug(pm_logger) << "Loaded plugins:" << m_loadedPlugins.size();
    if(m_loadedPlugins.isEmpty())
        return false;
    return true;
}

QStringList PluginManager::pluginFiles()
{
    return m_pluginFiles;
}

void PluginManager::loadPluginList()
{
    auto pathList = PlatformHelper::appdataDirPaths();

    //For plugins in test env
    if(QDir(QCoreApplication::applicationDirPath() + "/plugins").exists())
    {
        qCDebug(pm_logger) << "Found plugins in test env, using debug path";
        pathList.clear();
        pathList.append(QCoreApplication::applicationDirPath() + "/plugins");
    }
    //Loops every path and looks for libs
    for (QString pathName : pathList)
    {
        if(!pathName.endsWith("/plugins"))
            pathName = pathName + "/plugins";
        //Skip if the path doesnt exits
        QDir pluginsDir(pathName);
        if(!pluginsDir.exists())
            continue;

        //We got a plugin path, lets loop over the files
        for (const QString &fileName : pluginsDir.entryList(QDir::Files)) {
            //Skip non libraries
            if(!fileName.endsWith(".so"))
                continue;
            //Try to load the plugin
            auto pluginLoader = new QPluginLoader(pluginsDir.absoluteFilePath(fileName), this);
            QObject *pluginInstance = pluginLoader->instance();
            if (pluginInstance) {
                //Check if the plugin is a KIOTPlugin
                auto *kiotPlugin = qobject_cast<KIOTPluginInterface *>(pluginInstance);
                if (kiotPlugin) {
                    //Add the plugin path to the list
                    qCDebug(pm_logger) << "Found plugin: " << fileName << " in " << pathName;
                    if(!m_pluginFiles.contains(pluginsDir.absoluteFilePath(fileName)))
                        m_pluginFiles.append(pluginsDir.absoluteFilePath(fileName));
                }
                pluginLoader->unload();
            }
            pluginLoader->deleteLater();
        }
    }
    qCDebug(pm_logger) << "Found " << m_pluginFiles.count() << " plugins";
}

