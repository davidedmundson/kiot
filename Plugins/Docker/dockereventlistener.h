// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QThread>
#include <QVariantMap>
#include <QString>
#include <atomic>

/**
 * @file dockereventlistener.h
 * @brief Background thread for listening to real-time Docker socket events.
 */

/**
 * @class DockerEventListener
 * @brief Monitors Docker container lifecycle events via the Docker UNIX socket.
 *
 * @details Runs in a dedicated background thread to capture container state changes
 *          (such as start, stop, die, etc.) without blocking the main application event loop.
 *          Emits signals when valid container events are detected.
 */
class DockerEventListener : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a DockerEventListener instance.
     * @param parent Optional parent QObject.
     * @param socketPath The Docker socket to listen on (another one in tests).
     */
    explicit DockerEventListener(QObject *parent = nullptr, const QString &socketPath = QStringLiteral("/var/run/docker.sock"));

    /**
     * @brief Safely signals the background thread to stop listening and exit.
     */
    void stop();

Q_SIGNALS:
    /**
     * @brief Emitted whenever a valid container event is parsed from the Docker stream.
     * @param name The name of the Docker container.
     * @param attrs A map containing event attributes (e.g., status, id, image).
     */
    void containerEvent(const QString &name, const QVariantMap &attrs);

protected:
    /**
     * @brief Thread execution entry point.
     * @details Connects to the Docker socket, sends the event stream HTTP request,
     *          and continuously reads incoming event lines until stopped.
     */
    void run() override;

private:
    /**
     * @brief Parses a single raw line of JSON data from the Docker event stream.
     * @param line Raw QByteArray containing the event line.
     * @return bool True if the event was successfully parsed and relates to a container.
     */
    bool processEventLine(const QByteArray &line);

    /// Path of the Docker UNIX socket.
    const QString m_socketPath;

    /// Atomic flag used to safely signal the background thread to terminate its loop.
    std::atomic<bool> m_stop{false};
};