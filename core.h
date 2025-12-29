// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include "systray.h"
#include "servicemanager.h"
#include <KSharedConfig>
#include <QApplication>
#include <QMqttSubscription>
#include <QObject>
#include <QVariantMap>

class QMqttClient;
class ConnectedNode;

struct IntegrationFactory {
    QString name;
    std::function<void()> factory;
    bool onByDefault = true; // ny flag for default enabled
};



class HaControl : public QObject
{
    Q_OBJECT
public:
    HaControl();
    ~HaControl();

    void validateStartup();
    static QMqttClient *mqttClient()
    {
        return s_self->m_client;
    }

    static bool registerIntegrationFactory(const QString &name, std::function<void()> plugin, bool onByDefault = true);

private:
    void doConnect();
    void loadIntegrations(KSharedConfigPtr config);
    bool ensureConfigDefaults(KSharedConfigPtr config);
    static QList<IntegrationFactory> s_integrations;
    static HaControl *s_self;
    QMqttClient *m_client;
    ConnectedNode *m_connectedNode;
    SystemTray *m_systemTray;
    ServiceManager *m_serviceManager;
};

// clang-format off

// Macro for integrations
#define REGISTER_INTEGRATION(nameStr, func, onByDefault) \
static bool dummy##func = HaControl::registerIntegrationFactory(nameStr, [](){ func(); }, onByDefault);

// clang-format on
