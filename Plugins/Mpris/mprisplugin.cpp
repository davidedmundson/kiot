// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mprisplugin.h"
#include <QCoreApplication>

using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger,Mpris)

MprisPlugin::MprisPlugin(QObject *parent)
    : QObject(parent)
{
}

QString MprisPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString MprisPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl MprisPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber MprisPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool MprisPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasMprisBase = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.mpris.MediaPlayer2", "talk");
        bool hasMprisWildcard = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.mpris.MediaPlayer2.*", "talk");

        if (!hasMprisBase || !hasMprisWildcard) {
            qCWarning(plugin_logger) << "Missing required Flatpak D-Bus permissions for MPRIS integration.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --talk-name=org.mpris.MediaPlayer2 --talk-name=org.mpris.MediaPlayer2.* " + appId;
            return false;
        }
    }

    return true;
}

bool MprisPlugin::enabledByDefault()
{
    if (!checkCompatibility()) {
        return false;
    }
    return true; 
}

bool MprisPlugin::startPlugin()
{
    if(m_multiplexer)
        stopPlugin();
    m_multiplexer = new MprisMultiplexer(this);

    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool MprisPlugin::stopPlugin()
{
    if(m_multiplexer)
    {
        m_multiplexer->deleteLater();
        m_multiplexer = nullptr;
    }
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}

#include "mprisplugin.moc"