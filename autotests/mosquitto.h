// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

namespace KiotTest
{

/**
 * A mosquitto broker of the test's own on 127.0.0.1, anonymous and without persistence, so a
 * restart starts empty (no retained messages).
 */
class MosquittoBroker
{
public:
    ~MosquittoBroker()
    {
        stop();
    }

    static QString executable()
    {
        QString path = QStandardPaths::findExecutable(QStringLiteral("mosquitto"));
        if (path.isEmpty()) {
            path = QStandardPaths::findExecutable(QStringLiteral("mosquitto"), {QStringLiteral("/usr/sbin"), QStringLiteral("/usr/local/sbin")});
        }
        return path;
    }

    /** Starts the broker, on a free port the first time and on the same port after stop(). */
    bool start()
    {
        if (m_port == 0) {
            QTcpServer probe;
            if (!probe.listen(QHostAddress::LocalHost, 0)) {
                return false;
            }
            m_port = probe.serverPort();
        }
        const QString config = m_dir.filePath(QStringLiteral("mosquitto.conf"));
        QFile file(config);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        file.write(QStringLiteral("listener %1 127.0.0.1\nallow_anonymous true\npersistence false\n").arg(m_port).toUtf8());
        file.close();

        m_process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        m_process.start(executable(), {QStringLiteral("-c"), config});
        if (!m_process.waitForStarted()) {
            return false;
        }
        return QTest::qWaitFor(
            [this] {
                QTcpSocket socket;
                socket.connectToHost(QHostAddress::LocalHost, m_port);
                return socket.waitForConnected(100);
            },
            5000);
    }

    void stop()
    {
        if (m_process.state() == QProcess::NotRunning) {
            return;
        }
        m_process.terminate();
        if (!m_process.waitForFinished(5000)) {
            m_process.kill();
            m_process.waitForFinished();
        }
    }

    quint16 port() const
    {
        return m_port;
    }

private:
    QTemporaryDir m_dir;
    QProcess m_process;
    quint16 m_port = 0;
};

} // namespace KiotTest
