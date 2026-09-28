// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dockerplugin.h"
#include "dockerswitch.h"

#include <KIOTShared/kiotshared.h>
#include "core/core.h"

using KIOTShared::PlatformHelper;

#include <KUser>
#include <QLocalSocket>

DEFINE_PLUGIN_LOGGER(docker, Docker)

static const char *DOCKER_SOCKET_PATH = "/var/run/docker.sock";

DockerPlugin::DockerPlugin(QObject *parent)
    : QObject(parent)
{
}

DockerPlugin::~DockerPlugin()
{
    stopPlugin();
}

QString DockerPlugin::name() const
{
    return QString(PLUGIN_NAME);
}

QString DockerPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}

QUrl DockerPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}

QVersionNumber DockerPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool DockerPlugin::isUserInDockerGroup() const
{
    if (PlatformHelper::isFlatpak())
        return true;
    KUser currentUser;
    const QList<KUserGroup> groups = currentUser.groups();

    for (const KUserGroup &group : groups) {
        if (group.name() == QStringLiteral("docker")) {
            return true;
        }
    }
    return false;
}

bool DockerPlugin::isDockerAvailable() const
{
    QLocalSocket testSocket;
    testSocket.connectToServer(DOCKER_SOCKET_PATH, QIODevice::ReadWrite);
    const bool available = testSocket.waitForConnected(1000);
    if (available) {
        testSocket.disconnectFromServer();
    }
    return available;
}

bool DockerPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasDockerSocket = PlatformHelper::checkFlatpakFeature("Context", "filesystems", "/run/docker.sock");
        if (!hasDockerSocket) {
            qCWarning(docker) << "Missing required Flatpak permission to use the docker socker /run/docker.sock";
            qCInfo(docker) << "To fix this, run:";
            qCInfo(docker) << "  flatpak --user override --filesystem=/run/docker.sock " + appId;
            return false;
        }
    }
    if (!isUserInDockerGroup()) {
        qCWarning(docker) << "User is not in the 'docker' group! Docker integration will not function properly.";
        return false;
    }
    if (!isDockerAvailable()) {
        qCWarning(docker) << "Docker socket not available.";
        return false;
    }
    return true;
}


bool DockerPlugin::enabledByDefault()
{
    if(!checkCompatibility())
        return false;

    return true;
}


bool DockerPlugin::startPlugin()
{
    if (m_dockerSwitch)
        stopPlugin();

    if (!checkCompatibility()) {
        return false;
    }

    m_dockerSwitch = new DockerSwitch(this);
    return true;
}

bool DockerPlugin::stopPlugin()
{
    if (m_dockerSwitch) {
        delete m_dockerSwitch;
        m_dockerSwitch = nullptr;
        qCInfo(docker) << "Stopped Docker plugin and cleaned up resources.";
    }
    return true;
}

#include "dockerplugin.moc"