// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include <KIOTShared/kiotshared.h>
#include "plugins/pluginmanager.h"
#include <KSharedConfig>
#include <QCoreApplication>
#include <QMqttSubscription>
#include <QObject>
#include <QVariantMap>
#include <QPluginLoader>

// Trekk inn PlatformHelper i globalt scope (eller bruk KIOTShared::PlatformHelper i koden)
using KIOTShared::PlatformHelper;
using KIOTShared::Plugins::KIOTPluginInterface;

class QMqttClient;
class ConnectedNode;
class MainWindow;
struct LoadedPlugin {
        KIOTPluginInterface *interface = nullptr;
        QPluginLoader *loader = nullptr;
    };
class HaControl : public QObject
{
    Q_OBJECT
public:
    HaControl();
    ~HaControl();


    static bool registerIntegrationFactory(const QString &name, std::function<void()> plugin, bool onByDefault = true);

private:
    void validateStartup(bool autostart);
    bool validateConfig();
    void loadIntegrations();

    QList<LoadedPlugin> m_loadedPlugins;
    static HaControl *s_self;
    PluginManager *m_pluginManager = nullptr;
    ConnectedNode *m_connectedNode = nullptr;
    MainWindow *m_mainWindow = nullptr;
};

// clang-format off



// clang-format on