// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file template.cpp
 * @brief Implementation of the KIOT plugin template.
 */

#include "template.h"
#include <QString>
DEFINE_PLUGIN_LOGGER(tplogger, TemplatePlugin) //Change TeplatePlugin to you plugin name for better logs

TemplatePlugin::TemplatePlugin(QObject *parent)
    : QObject(parent)
{
}

TemplatePlugin::~TemplatePlugin()
{
    stopPlugin();
}

QString TemplatePlugin::name() const
{
    return QStringLiteral(PLUGIN_NAME);
}

QString TemplatePlugin::description() const
{
    return QStringLiteral(PLUGIN_DESCRIPTION);
}

QUrl TemplatePlugin::url() const
{
    return QUrl(QStringLiteral(PLUGIN_DOMAIN));
}

QVersionNumber TemplatePlugin::version() const
{
    QString version = QStringLiteral(PLUGIN_VERSION);
    return QVersionNumber::fromString(version);
}

bool TemplatePlugin::checkCompatibility()
{

    return true;
}

bool TemplatePlugin::enabledByDefault()
{
    if(!checkCompatibility())
        return false;

    return true;
}

bool TemplatePlugin::startPlugin()
{
    if(!checkCompatibility())
    {
        qCInfo(tplogger) << "mesa so sowwi, " << name() << "not supported on your system";
        return false;
    }
    if(m_dndSensor)
        stopPlugin();
    m_dndSensor = new BinarySensor(this);
    m_dndSensor->setId(QStringLiteral("Teplate_FlatpakExtension"));
    m_dndSensor->setName(QStringLiteral("Test Flatpak Extensions"));

    m_dndSensor->setState(true);

    qCInfo(tplogger) << name() << "plugin started successfully";
    return true;
}

bool TemplatePlugin::stopPlugin()
{
    if(m_dndSensor)
    {
        m_dndSensor->deleteLater();
        m_dndSensor = nullptr;
    }
    qCInfo(tplogger) << name() << "plugin stopped";
    return true;
}
