// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

// The Docker plugin's event listener, against a fake Docker socket that answers GET /events the
// way dockerd does over HTTP/1.0: the response headers, then one JSON event per line.

#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QTest>

#include "dockereventlistener.h"

static QByteArray containerEvent(const QByteArray &action, const QByteArray &name)
{
    return R"({"status":")" + action + R"(","id":"0123abcd","from":"nginx","Type":"container","Action":")" + action
        + R"(","Actor":{"ID":"0123abcd","Attributes":{"image":"nginx","name":")" + name + R"("}},"scope":"local","time":1})" + "\n";
}

class DockerEventListenerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_server = new QLocalServer(this);
        const QString path = m_dir.filePath(QStringLiteral("docker.sock"));
        QLocalServer::removeServer(path);
        QVERIFY(m_server->listen(path));

        m_listener = new DockerEventListener(nullptr, m_server->fullServerName());
        m_events.clear();
        connect(m_listener, &DockerEventListener::containerEvent, this, [this](const QString &name, const QVariantMap &attrs) {
            m_events << name + QLatin1Char(':') + attrs.value(QStringLiteral("status")).toString();
        });
        m_listener->start();

        QVERIFY(m_server->waitForNewConnection(5000));
        m_docker = m_server->nextPendingConnection();
        QVERIFY(m_docker);
        QTRY_VERIFY(m_docker->bytesAvailable() > 0);
        QVERIFY(m_docker->readAll().startsWith("GET /events HTTP/1.0\r\n"));
    }

    void cleanup()
    {
        m_listener->stop();
        QVERIFY(m_listener->wait(5000));
        delete m_listener;
        m_listener = nullptr;
        delete m_server;
        m_server = nullptr;
    }

    void testHeadersAndBurstInOneWrite()
    {
        // what `docker stop web` sends at once; the network event and the nameless one are skipped
        m_docker->write("HTTP/1.0 200 OK\r\nApi-Version: 1.45\r\nContent-Type: application/json\r\n"
                        "Docker-Experimental: false\r\nOstype: linux\r\nServer: Docker/26.1.0 (linux)\r\n"
                        "Date: Wed, 25 Nov 2026 12:00:00 GMT\r\n\r\n"
                        + containerEvent("kill", "web") + containerEvent("die", "web")
                        + R"({"Type":"network","Action":"disconnect","Actor":{"ID":"n1","Attributes":{"name":"bridge"}}})" "\n"
                        + R"({"Type":"container","Action":"destroy","Actor":{"ID":"x","Attributes":{}}})" "\n"
                        + containerEvent("stop", "web"));
        QVERIFY(m_docker->waitForBytesWritten(5000));

        QTRY_COMPARE(m_events, (QStringList{QStringLiteral("web:kill"), QStringLiteral("web:die"), QStringLiteral("web:stop")}));
    }

    void testLineSplitAcrossWrites()
    {
        m_docker->write("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n\r\n");
        const QByteArray event = containerEvent("start", "db");
        m_docker->write(event.left(40));
        QVERIFY(m_docker->waitForBytesWritten(5000));
        QTest::qWait(300);
        QVERIFY(m_events.isEmpty());

        m_docker->write(event.mid(40) + containerEvent("start", "cache"));
        QVERIFY(m_docker->waitForBytesWritten(5000));
        QTRY_COMPARE(m_events, (QStringList{QStringLiteral("db:start"), QStringLiteral("cache:start")}));
    }

private:
    QTemporaryDir m_dir;
    QLocalServer *m_server = nullptr;
    QLocalSocket *m_docker = nullptr;
    DockerEventListener *m_listener = nullptr;
    QStringList m_events;
};

QTEST_GUILESS_MAIN(DockerEventListenerTest)

#include "dockereventlistenertest.moc"
