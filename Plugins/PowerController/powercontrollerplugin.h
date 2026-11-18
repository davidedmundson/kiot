// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QObject>
#include <KIOTShared/kiotshared.h>

class SuspendBlockers;

using KIOTShared::Entities::Button;
using KIOTShared::PlatformHelper;
using KIOTShared::Plugins::KIOTPluginInterface;

class PowControlelrPlugin : public QObject, public KIOTShared::Plugins::KIOTPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID KIOTPluginInterface_iid FILE "plugin.json")
    Q_INTERFACES(KIOTShared::Plugins::KIOTPluginInterface)

public:
    PowControlelrPlugin(QObject *parent = nullptr);
    ~PowControlelrPlugin() override = default;

    QString name() const override;
    QString description() const override;
    QUrl url() const override;
    QVersionNumber version() const override;
    bool checkCompatibility() override;
    bool enabledByDefault() override;
    bool startPlugin() override;
    bool stopPlugin() override;

private:
    void setupPowerButtons();
    Button *m_powerButton = nullptr;
    Button *m_restartButton = nullptr;
    Button *m_suspendButton = nullptr;
    Button *m_hibernateButton = nullptr;
    SuspendBlockers *m_suspendBlockers = nullptr;
    
};