// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file template.cpp
 * @brief Implementation of the KIOT plugin template.
 */

#include "sendspinplugin.h"
#include <QHostInfo>
#include <QString>
#include <KSharedConfig>
#include <KConfigGroup>
DEFINE_PLUGIN_LOGGER(tplogger, SendSpinPlugin) //Change TeplatePlugin to you plugin name for better logs

SendSpinPlugin::SendSpinPlugin(QObject *parent)
    : QObject(parent)
{
}

SendSpinPlugin::~SendSpinPlugin()
{
    stopPlugin();
}

QString SendSpinPlugin::name() const
{
    return QStringLiteral(PLUGIN_NAME);
}

QString SendSpinPlugin::description() const
{
    return QStringLiteral(PLUGIN_DESCRIPTION);
}

QUrl SendSpinPlugin::url() const
{
    return QUrl(QStringLiteral(PLUGIN_DOMAIN));
}

QVersionNumber SendSpinPlugin::version() const
{
    QString version = QStringLiteral(PLUGIN_VERSION);
    return QVersionNumber::fromString(version);
}

bool SendSpinPlugin::checkCompatibility()
{

    return true;
}

bool SendSpinPlugin::enabledByDefault()
{
    if(!checkCompatibility())
        return false;

    return true;
}

bool SendSpinPlugin::startPlugin()
{
    if(!checkCompatibility())
    {
        qCInfo(tplogger) << "mesa so sowwi, " << name() << "not supported on your system";
        return false;
    }
    if(m_sendspinClient)
        stopPlugin();
    auto conf = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    if (!conf->hasGroup("sendspin")) {
        KConfigGroup group(conf, "sendspin");
        group.writeEntry("url", "ws://homeassistant.local:8927/sendspin");
        conf->sync();
    }
    KConfigGroup group(conf, "sendspin");
    QString url = group.readEntry("url", "ws://homeassistant.local:8927/sendspin");
    m_sendspinClient = new SendspinDesktopClient("KIOT " + QHostInfo::localHostName().toLower());
    //TODO make url part of config file
    m_sendspinClient->connectToServer(url); // Evt. send med en URL hvis du har det
    qCInfo(tplogger) << name() << "plugin started successfully";
    return true;
}

bool SendSpinPlugin::stopPlugin()
{
    if (m_sendspinClient) {
        m_sendspinClient->disconnectFromServer();
        delete m_sendspinClient;
        m_sendspinClient = nullptr;
    }
    return true;
    qCInfo(tplogger) << name() << "plugin stopped";
    return true;
}
#include "sendspinplugin.moc"