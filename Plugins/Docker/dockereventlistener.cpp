// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dockereventlistener.h"
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonArray>
#include <KIOTShared/kiotshared.h>

using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(docker_event_logs, Docker)
DockerEventListener::DockerEventListener(QObject *parent, const QString &socketPath)
    : QThread(parent)
    , m_socketPath(socketPath)
{
}

void DockerEventListener::stop()
{
    m_stop = true;
    quit();
}

void DockerEventListener::run()
{
    QLocalSocket socket;
    socket.connectToServer(m_socketPath, QIODevice::ReadWrite);
    if (!socket.waitForConnected(1000)) {
        qCWarning(docker_event_logs) << "Failed to connect to Docker socket";
        return;
    }

    const QByteArray request = "GET /events HTTP/1.0\r\nHost: localhost\r\n\r\n";
    if (socket.write(request) != request.size()) {
        qCWarning(docker_event_logs) << "Failed to write request to socket";
        return;
    }
    socket.flush();

    qCInfo(docker_event_logs) << "Event listener started";

    while (!m_stop && socket.state() == QLocalSocket::ConnectedState) {
        if (!socket.waitForReadyRead(1000)) {
            continue;
        }

        // waitForReadyRead() only wakes up for new data, so every complete line has to be read
        // now: one wakeup can bring several (the HTTP headers, a burst like kill/die/stop)
        while (socket.canReadLine()) {
            const QByteArray line = socket.readLine().trimmed();
            if (line.isEmpty())
                continue;

            processEventLine(line);
        }
    }

    socket.disconnectFromServer();
    qCInfo(docker_event_logs) << "Event listener stopped";
}

bool DockerEventListener::processEventLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return false;
    }

    if (!doc.isObject())
        return false;

    const QJsonObject obj = doc.object();
    if (obj.value("Type").toString() != "container")
        return false;

    const QJsonObject actor = obj.value("Actor").toObject();
    const QJsonObject attrsObj = actor.value("Attributes").toObject();
    const QString name = attrsObj.value("name").toString();

    if (name.isEmpty())
        return false;

    QVariantMap attrs;
    attrs["status"] = obj.value("status").toString();
    attrs["id"] = obj.value("id").toString();
    attrs["image"] = attrsObj.value("image").toString();

    emit containerEvent(name, attrs);
    return true;
}