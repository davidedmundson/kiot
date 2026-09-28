// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "powercontrollerplugin.h"
#include <QDBusConnection>
#include <QCoreApplication>
#include "login1_manager_interface.h"


DEFINE_PLUGIN_LOGGER(plugin_logger,PLUGIN_NAME)

PowControlelrPlugin::PowControlelrPlugin(QObject *parent)
    : QObject(parent)
{
}

QString PowControlelrPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString PowControlelrPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl PowControlelrPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber PowControlelrPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool PowControlelrPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasLogin1Talk = PlatformHelper::checkFlatpakFeature("System Bus Policy", "org.freedesktop.login1", "talk");
        if (!hasLogin1Talk) {
            qCWarning(plugin_logger) << "Missing required Flatpak system D-Bus permission for systemd login1 (power control).";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --system-talk-name=org.freedesktop.login1 " + appId;
            return false;
        }
    }

    return true;
}

bool PowControlelrPlugin::enabledByDefault()
{
    if (!checkCompatibility()) {
        return false;
    }
    return true; 
}

bool PowControlelrPlugin::startPlugin()
{
    if(m_restartButton || m_hibernateButton || m_powerButton || m_suspendButton)
        stopPlugin();

    setupPowerButtons();
    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool PowControlelrPlugin::stopPlugin()
{
    if(m_suspendButton)
    {
        disconnect(m_suspendButton,nullptr,this,nullptr);
        m_suspendButton->deleteLater();
        m_suspendButton = nullptr;
    }

    if(m_hibernateButton)
    {
        disconnect(m_hibernateButton,nullptr,this,nullptr);
        m_hibernateButton->deleteLater();
        m_hibernateButton = nullptr;
    }

    if(m_restartButton)
    {
        disconnect(m_restartButton,nullptr,this,nullptr);
        m_restartButton->deleteLater();
        m_restartButton = nullptr;
    }

    if(m_powerButton)
    {
        disconnect(m_powerButton,nullptr,this,nullptr);
        m_powerButton->deleteLater();
        m_powerButton = nullptr;
    }
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}

void PowControlelrPlugin::setupPowerButtons()
{
        m_suspendButton = new Button(qApp);
        m_suspendButton->setId("suspend");
        m_suspendButton->setName("Suspend");

        QObject::connect(m_suspendButton, &Button::triggered, this, []() {
            OrgFreedesktopLogin1ManagerInterface logind(QStringLiteral("org.freedesktop.login1"),
                                                        QStringLiteral("/org/freedesktop/login1"),
                                                        QDBusConnection::systemBus());
            logind.Suspend(false).waitForFinished();
        });

        m_hibernateButton = new Button(qApp);
        m_hibernateButton->setId("hibernate");
        m_hibernateButton->setName("Hibernate");

        QObject::connect(m_hibernateButton, &Button::triggered, this, []() {
            OrgFreedesktopLogin1ManagerInterface logind(QStringLiteral("org.freedesktop.login1"),
                                                        QStringLiteral("/org/freedesktop/login1"),
                                                        QDBusConnection::systemBus());
            logind.Hibernate(false).waitForFinished();
        });

        m_powerButton = new Button(qApp);
        m_powerButton->setId("poweroff");
        m_powerButton->setName("Poweroff");

        QObject::connect(m_powerButton, &Button::triggered, this, []() {
            OrgFreedesktopLogin1ManagerInterface logind(QStringLiteral("org.freedesktop.login1"),
                                                        QStringLiteral("/org/freedesktop/login1"),
                                                        QDBusConnection::systemBus());
            logind.PowerOff(false).waitForFinished();
        });

        m_restartButton = new Button(qApp);
        m_restartButton->setId("restart");
        m_restartButton->setName("Restart");

        QObject::connect(m_restartButton, &Button::triggered, this, []() {
            OrgFreedesktopLogin1ManagerInterface logind(QStringLiteral("org.freedesktop.login1"),
                                                        QStringLiteral("/org/freedesktop/login1"),
                                                        QDBusConnection::systemBus());
            logind.Reboot(false).waitForFinished();
        });
}

#include "powercontrollerplugin.moc"