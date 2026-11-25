// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dockerswitch.h"
#include "dockereventlistener.h"

#include <KIOTShared/kiotshared.h>

using KIOTShared::Entities::Switch;
using KIOTShared::PlatformHelper;

#include <KConfigGroup>
#include <KSharedConfig>
#include <QDebug>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

DEFINE_PLUGIN_LOGGER(docker_switch_logs, Docker)

static const int SOCKET_TIMEOUT_MS = 10000;
static const char *DOCKER_SOCKET_PATH = "/var/run/docker.sock";

DockerSwitch::DockerSwitch(QObject *parent)
    : QObject(parent)
{
    if (!ensureConfigDefaults()) {
        qCDebug(docker_switch_logs) << "No active container images to monitor";
        return;
    }

    initializeSwitches();
    startEventListener();

    qCInfo(docker_switch_logs) << "Integration initialized with" << m_containers.size() << "containers";
}

DockerSwitch::~DockerSwitch()
{
    stopEventListener();
}

void DockerSwitch::initializeSwitches()
{
    const auto cfg = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig);
    const KConfigGroup grp = cfg->group("docker");

    for (const auto &key : grp.keyList()) {
        if (!grp.readEntry(key, false))
            continue;

        qCInfo(docker_switch_logs) << "Enabling control for container" << key;
        createContainerSwitch(key);
    }
}

void DockerSwitch::createContainerSwitch(const QString &name)
{
    auto *sw = new Switch(this);
    sw->setId("docker_" + name);
    sw->setName(name);
    sw->setDiscoveryConfig("icon", "mdi:docker");

    updateSwitch(name, sw);

    connect(sw, &Switch::stateChangeRequested, this, [this, name](bool state) {
        toggleContainer(name, state);
    });

    m_containers.append({name, sw});
}

void DockerSwitch::startEventListener()
{
    m_listener = new DockerEventListener(this);
    connect(m_listener, &DockerEventListener::containerEvent, this, &DockerSwitch::handleEvent, Qt::QueuedConnection);
    m_listener->start();
}

void DockerSwitch::stopEventListener()
{
    if (!m_listener)
        return;

    m_listener->stop();
    if (!m_listener->wait(3000)) {
        qCDebug(docker_switch_logs) << "Event listener did not stop gracefully, terminating";
        m_listener->terminate();
        m_listener->wait(1000);
    }
}

bool DockerSwitch::ensureConfigDefaults()
{
    const auto cfg = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig);
    KConfigGroup grp = cfg->group("docker");

    const QStringList currentContainers = listAllContainers();
    if (currentContainers.isEmpty()) {
        qCDebug(docker_switch_logs) << "No containers found";
        return false;
    }

    const QStringList configContainers = grp.keyList();
    bool configChanged = false;

    for (const QString &containerName : currentContainers) {
        if (!grp.hasKey(containerName)) {
            grp.writeEntry(containerName, false);
            configChanged = true;
            qCDebug(docker_switch_logs) << "Added new container to config:" << containerName;
        }
    }

    for (const QString &configContainer : configContainers) {
        if (!currentContainers.contains(configContainer)) {
            grp.deleteEntry(configContainer);
            configChanged = true;
            qCDebug(docker_switch_logs) << "Removed unavailable container from config:" << configContainer;
        }
    }

    if (configChanged) {
        cfg->sync();
        qCDebug(docker_switch_logs) << "Configuration updated with current containers";
    }

    return true;
}
bool DockerSwitch::callDockerSocket(const QByteArray &request, QByteArray &response)
{
    QLocalSocket socket;
    socket.connectToServer(DOCKER_SOCKET_PATH, QIODevice::ReadWrite);
    if (!socket.waitForConnected(1000)) {
        qCDebug(docker_switch_logs) << "Failed to connect to Docker socket";
        return false;
    }

    if (socket.write(request) != request.size()) {
        qCDebug(docker_switch_logs) << "Failed to write request to socket";
        return false;
    }

    socket.flush();

    response.clear();
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < SOCKET_TIMEOUT_MS) {
        if (socket.waitForReadyRead(500)) {
            response.append(socket.readAll());

        } else if (socket.state() == QLocalSocket::UnconnectedState) {
            break; 
        }
    }

    socket.disconnectFromServer();
    
    if (response.isEmpty()) {
        qCDebug(docker_switch_logs) << "Timeout or empty response from Docker socket";
        return false;
    }

    return true;
}
QStringList DockerSwitch::listAllContainers(int retries)
{
    QStringList names;
    QByteArray response;

    // all=1: stopped containers too
    const QByteArray request = "GET /containers/json?all=1 HTTP/1.0\r\n\r\n";

    while (retries-- > 0) {
        if (!callDockerSocket(request, response))
            continue;
        const QByteArray body = extractHttpBody(response);
        if (body.isEmpty())
            continue;

        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isArray()) {
            qCDebug(docker_switch_logs) << "Unexpected response format for container list, retries left:" << retries;
            continue;
        }

        for (const auto &value : doc.array()) {
            if (!value.isObject())
                continue;
            const QJsonArray namesArray = value.toObject()["Names"].toArray();
            if (namesArray.isEmpty())
                continue;

            QString name = namesArray.first().toString();
            if (name.startsWith("/"))
                name.remove(0, 1);
            if (!name.isEmpty())
                names.append(name);
        }
        break;
    }

    return names;
}

bool DockerSwitch::isRunning(const QString &name)
{
    QByteArray response;
    const QByteArray request = "GET /containers/json?all=0 HTTP/1.0\r\n\r\n";
    if (!callDockerSocket(request, response)) {
        return false;
    }

    const QByteArray body = extractHttpBody(response);
    if (body.isEmpty())
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray())
        return false;

    for (const auto &value : doc.array()) {
        if (!value.isObject())
            continue;

        const QJsonArray namesArray = value.toObject()["Names"].toArray();
        if (namesArray.isEmpty())
            continue;

        QString containerName = namesArray.first().toString();
        if (containerName.startsWith("/")) {
            containerName.remove(0, 1);
        }

        if (containerName == name) {
            return true;
        }
    }
    return false;
}

QByteArray DockerSwitch::extractHttpBody(const QByteArray &response)
{
    const int headerEnd = response.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        qCDebug(docker_switch_logs) << "Invalid HTTP response format";
        return QByteArray();
    }
    return response.mid(headerEnd + 4);
}

void DockerSwitch::toggleContainer(const QString &name, bool start)
{
    const QString action = start ? "start" : "stop";
    const QByteArray request =QStringLiteral("POST /containers/%1/%2 HTTP/1.0\r\n\r\n").arg(name, action).toUtf8();

    QByteArray response;
    if (!callDockerSocket(request, response)) {
        qCDebug(docker_switch_logs) << "Failed to" << action << "container" << name;
        return;
    }

    qCDebug(docker_switch_logs) << "Container" << name << (start ? "started" : "stopped");

    for (auto &containerInfo : m_containers) {
        if (containerInfo.name == name) {
            updateSwitch(name, containerInfo.sw);
            break;
        }
    }
}

void DockerSwitch::updateSwitch(const QString &name, Switch *sw)
{
    const bool running = isRunning(name);
    sw->setState(running);

    QByteArray response;
    const QByteArray request =QStringLiteral("GET /containers/%1/json HTTP/1.0\r\n\r\n").arg(name).toUtf8();

    if (!callDockerSocket(request, response)) {
        qCDebug(docker_switch_logs) << "Failed to get container details for" << name;
        return;
    }

    const QByteArray body = extractHttpBody(response);
    if (body.isEmpty()) {
        qCDebug(docker_switch_logs) << "Empty body extracted from response. Raw response was:" << response;
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (!doc.isObject()) {
        qCDebug(docker_switch_logs) << "JSON parse error:" << parseError.errorString() << "Body was:" << body;
        return;
    }

    const QJsonObject containerObj = doc.object();
    const QJsonObject config = containerObj["Config"].toObject();
    const QJsonObject state = containerObj["State"].toObject();
    const QJsonObject networkSettings = containerObj["NetworkSettings"].toObject();

    QVariantMap attributes;
    attributes["image"] = config["Image"].toString();
    attributes["status"] = state["Status"].toString();
    attributes["running"] = QVariant(state["Running"].toBool()).toString();
    attributes["created"] = containerObj["Created"].toString();
    attributes["ports"] = networkSettings["Ports"].toVariant();

    sw->setAttributes(attributes);
}

void DockerSwitch::handleEvent(const QString &name, const QVariantMap &attrs)
{
    Q_UNUSED(attrs)

    for (auto &containerInfo : m_containers) {
        if (containerInfo.name == name) {
            updateSwitch(name, containerInfo.sw);
            break;
        }
    }
}