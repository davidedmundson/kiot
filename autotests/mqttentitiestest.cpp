// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

// The entities against a real broker: TransportManager connects to a mosquitto of the test's own,
// an observer client subscribed to '#' sees everything kiot publishes and sends the commands Home
// Assistant would send. Checks the discovery configs, states, attributes and command handling of
// every entity type, re-registration, reconnects and a broker restart.
//
// Needs mosquitto; skips without it unless KIOT_MQTT_TESTS=required.

#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttClient>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

#include <optional>

#include <KIOTShared/kiotshared.h>

#include "mosquitto.h"
#include "testenv.h"

using namespace KIOTShared::Entities;
using KIOTShared::Transport::TransportManager;

static KiotTest::IsolatedEnvironment s_env;

namespace
{
struct Message {
    QString topic;
    QByteArray payload;
};

QString host()
{
    return QHostInfo::localHostName().toLower();
}

QString baseTopic(const QString &id)
{
    return QStringLiteral("kiot/%1/%2").arg(host(), id);
}

QString discoveryTopic(const QString &type, const QString &id)
{
    return QStringLiteral("homeassistant/%1/%2/%3/config").arg(type, host(), id);
}
} // namespace

class MqttEntitiesTest : public QObject
{
    Q_OBJECT

private:
    KiotTest::MosquittoBroker m_broker;
    QMqttClient *m_observer = nullptr;
    TransportManager *m_transport = nullptr;
    QList<Message> m_messages;

    bool connectObserver()
    {
        m_observer->connectToHost();
        if (!QTest::qWaitFor([this] {
                return m_observer->state() == QMqttClient::Connected;
            })) {
            return false;
        }
        auto subscription = m_observer->subscribe(QStringLiteral("#"));
        if (!subscription) {
            return false;
        }
        connect(subscription, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage &message) {
            m_messages.append({message.topic().name(), message.payload()});
        });
        return QTest::qWaitFor([subscription] {
            return subscription->state() == QMqttSubscription::Subscribed;
        });
    }

    qsizetype mark() const
    {
        return m_messages.size();
    }

    /** The first payload on @p topic that arrived after @p since, waiting for it if need be. */
    std::optional<QByteArray> waitForMessage(const QString &topic, qsizetype since, int timeout = 5000)
    {
        std::optional<QByteArray> found;
        std::ignore = QTest::qWaitFor(
            [&] {
                for (qsizetype i = since; i < m_messages.size(); ++i) {
                    if (m_messages.at(i).topic == topic) {
                        found = m_messages.at(i).payload;
                        return true;
                    }
                }
                return false;
            },
            timeout);
        return found;
    }

    /** All payloads on @p topic since @p since, without waiting. */
    QList<QByteArray> messagesOn(const QString &topic, qsizetype since) const
    {
        QList<QByteArray> result;
        for (qsizetype i = since; i < m_messages.size(); ++i) {
            if (m_messages.at(i).topic == topic) {
                result.append(m_messages.at(i).payload);
            }
        }
        return result;
    }

    QJsonObject waitForJson(const QString &topic, qsizetype since)
    {
        const auto payload = waitForMessage(topic, since);
        if (!payload) {
            return {};
        }
        return QJsonDocument::fromJson(*payload).object();
    }

    /** Waits until kiot's own client has its subscription to @p topic acknowledged by the broker. */
    bool waitSubscribed(const QString &topic)
    {
        // subscribe() hands back the active subscription of that topic, it doesn't add one
        auto subscription = TransportManager::mqttClient()->subscribe(topic);
        return subscription && QTest::qWaitFor([subscription] {
                   return subscription->state() == QMqttSubscription::Subscribed;
               });
    }

    void send(const QString &topic, const QByteArray &payload)
    {
        m_observer->publish(topic, payload, 0, false);
    }

    bool waitConnected(int timeout = 5000)
    {
        return QTest::qWaitFor(
            [] {
                return TransportManager::mqttClient()->state() == QMqttClient::Connected;
            },
            timeout);
    }

private Q_SLOTS:
    void initTestCase()
    {
        if (KiotTest::MosquittoBroker::executable().isEmpty()) {
            if (qEnvironmentVariable("KIOT_MQTT_TESTS") == QLatin1String("required")) {
                QFAIL("mosquitto not found, and KIOT_MQTT_TESTS=required");
            }
            QSKIP("mosquitto not found");
        }
        QVERIFY(m_broker.start());

        // without a host the transport gives up, and leaves no client behind
        KiotTest::writeGeneralConfig({{QStringLiteral("host"), QString()}});
        QPointer<TransportManager> unconfigured = new TransportManager(this);
        QVERIFY(!TransportManager::mqttClient());
        QTRY_VERIFY(unconfigured.isNull());

        // the password of a first run that hasn't been configured yet: not a reason to give up
        KiotTest::writeGeneralConfig({{QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                                      {QStringLiteral("port"), m_broker.port()},
                                      {QStringLiteral("user"), QString()},
                                      {QStringLiteral("password"), QStringLiteral("mqtt-password-here")}});

        m_observer = new QMqttClient(this);
        m_observer->setHostname(QStringLiteral("127.0.0.1"));
        m_observer->setPort(m_broker.port());
        QVERIFY(connectObserver());

        m_transport = new TransportManager(this);
        QVERIFY(TransportManager::mqttClient());
        m_transport->doConnect();
        QVERIFY(waitConnected());
    }

    void testSwitch()
    {
        const qsizetype since = mark();
        const QString base = baseTopic(QStringLiteral("test_switch"));

        Switch sw;
        sw.setId(QStringLiteral("Test Switch"));
        sw.setName(QStringLiteral("Test Switch"));
        sw.setState(true);
        sw.runtimeRegistration();

        const QJsonObject config = waitForJson(discoveryTopic(QStringLiteral("switch"), QStringLiteral("test_switch")), since);
        QCOMPARE(config.value(QStringLiteral("name")).toString(), QStringLiteral("Test Switch"));
        QCOMPARE(config.value(QStringLiteral("state_topic")).toString(), base);
        QCOMPARE(config.value(QStringLiteral("command_topic")).toString(), base + QStringLiteral("/set"));
        QCOMPARE(config.value(QStringLiteral("payload_on")).toString(), QStringLiteral("true"));
        QCOMPARE(config.value(QStringLiteral("payload_off")).toString(), QStringLiteral("false"));
        QCOMPARE(config.value(QStringLiteral("json_attributes_topic")).toString(), base + QStringLiteral("/attributes"));
        QCOMPARE(config.value(QStringLiteral("availability_topic")).toString(), QStringLiteral("kiot/%1/connected").arg(host()));
        QCOMPARE(config.value(QStringLiteral("payload_available")).toString(), QStringLiteral("on"));
        QCOMPARE(config.value(QStringLiteral("payload_not_available")).toString(), QStringLiteral("off"));
        QCOMPARE(config.value(QStringLiteral("unique_id")).toString(), QStringLiteral("linux_ha_control_%1_test_switch").arg(host()));
        const QJsonObject device = config.value(QStringLiteral("device")).toObject();
        QCOMPARE(device.value(QStringLiteral("identifiers")).toString(), QStringLiteral("linux_ha_bridge_%1").arg(host()));
        QCOMPARE(device.value(QStringLiteral("sw_version")).toString(), QStringLiteral(PROJECT_VERSION));

        QCOMPARE(waitForMessage(base, since), QByteArray("true"));
        QCOMPARE(waitForMessage(QStringLiteral("kiot/%1/connected").arg(host()), since), QByteArray("on"));

        QVERIFY(waitSubscribed(base + QStringLiteral("/set")));
        QSignalSpy requested(&sw, &Switch::stateChangeRequested);
        send(base + QStringLiteral("/set"), "false");
        send(base + QStringLiteral("/set"), "ON"); // not a payload of this switch: ignored
        send(base + QStringLiteral("/set"), "true");
        QTRY_COMPARE(requested.count(), 2);
        QCOMPARE(requested.at(0).at(0).toBool(), false);
        QCOMPARE(requested.at(1).at(0).toBool(), true);

        // the switch reports what the plugin sets, not what was asked for
        const qsizetype beforeState = mark();
        sw.setState(false);
        QCOMPARE(waitForMessage(base, beforeState), QByteArray("false"));
    }

    void testDiscoveryAndStateAreRetained()
    {
        Sensor sensor;
        sensor.setId(QStringLiteral("retained_sensor"));
        sensor.setName(QStringLiteral("Retained"));
        sensor.setState(QStringLiteral("42"));
        sensor.runtimeRegistration();
        QVERIFY(waitForMessage(discoveryTopic(QStringLiteral("sensor"), QStringLiteral("retained_sensor")), 0));

        // a client that connects later (Home Assistant after a restart) still gets both
        QMqttClient late;
        late.setHostname(QStringLiteral("127.0.0.1"));
        late.setPort(m_broker.port());
        late.connectToHost();
        QTRY_COMPARE(late.state(), QMqttClient::Connected);
        QHash<QString, QByteArray> retained;
        for (const QString &topic : {discoveryTopic(QStringLiteral("sensor"), QStringLiteral("retained_sensor")), baseTopic(QStringLiteral("retained_sensor"))}) {
            auto subscription = late.subscribe(topic);
            QVERIFY(subscription);
            connect(subscription, &QMqttSubscription::messageReceived, this, [&retained](const QMqttMessage &message) {
                if (message.retain()) {
                    retained.insert(message.topic().name(), message.payload());
                }
            });
        }
        QTRY_COMPARE(retained.size(), 2);
        QCOMPARE(retained.value(baseTopic(QStringLiteral("retained_sensor"))), QByteArray("42"));
        QCOMPARE(QJsonDocument::fromJson(retained.value(discoveryTopic(QStringLiteral("sensor"), QStringLiteral("retained_sensor"))))
                     .object()
                     .value(QStringLiteral("state_topic"))
                     .toString(),
                 baseTopic(QStringLiteral("retained_sensor")));
    }

    void testBinarySensorAndAttributes()
    {
        const qsizetype since = mark();
        const QString base = baseTopic(QStringLiteral("test_binary"));

        BinarySensor sensor;
        sensor.setId(QStringLiteral("test_binary"));
        sensor.setName(QStringLiteral("Binary"));
        sensor.setDiscoveryConfig(QStringLiteral("device_class"), QStringLiteral("occupancy"));
        sensor.runtimeRegistration();

        const QJsonObject config = waitForJson(discoveryTopic(QStringLiteral("binary_sensor"), QStringLiteral("test_binary")), since);
        QCOMPARE(config.value(QStringLiteral("state_topic")).toString(), base);
        QCOMPARE(config.value(QStringLiteral("payload_on")).toString(), QStringLiteral("true"));
        QCOMPARE(config.value(QStringLiteral("device_class")).toString(), QStringLiteral("occupancy"));
        QCOMPARE(waitForMessage(base, since), QByteArray("false"));

        const qsizetype beforeOn = mark();
        sensor.setState(true);
        QCOMPARE(waitForMessage(base, beforeOn), QByteArray("true"));
        QVERIFY(sensor.state());

        const qsizetype beforeAttributes = mark();
        sensor.setAttributes({{QStringLiteral("flag"), true},
                              {QStringLiteral("when"), QDateTime(QDate(2025, 1, 15), QTime(10, 30), QTimeZone::UTC)},
                              {QStringLiteral("list"), QVariantList{1, 2}},
                              {QStringLiteral("map"), QVariantMap{{QStringLiteral("k"), QStringLiteral("v")}}},
                              {QStringLiteral("count"), 3}});
        const QJsonObject attributes = waitForJson(base + QStringLiteral("/attributes"), beforeAttributes);
        QCOMPARE(attributes.value(QStringLiteral("flag")).toString(), QStringLiteral("true"));
        QCOMPARE(attributes.value(QStringLiteral("when")).toString(), QStringLiteral("2025-01-15T10:30:00Z"));
        QCOMPARE(attributes.value(QStringLiteral("list")).toArray(), QJsonArray({1, 2}));
        QCOMPARE(attributes.value(QStringLiteral("map")).toObject(), QJsonObject({{QStringLiteral("k"), QStringLiteral("v")}}));
        QCOMPARE(attributes.value(QStringLiteral("count")).toInt(), 3);
    }

    void testSelect()
    {
        const qsizetype since = mark();
        const QString base = baseTopic(QStringLiteral("test_select"));

        Select select;
        select.setId(QStringLiteral("test_select"));
        select.setName(QStringLiteral("Select"));
        select.setOptions({QStringLiteral("a"), QStringLiteral("b")});
        select.setState(QStringLiteral("a"));
        select.runtimeRegistration();

        const QJsonObject config = waitForJson(discoveryTopic(QStringLiteral("select"), QStringLiteral("test_select")), since);
        QCOMPARE(config.value(QStringLiteral("options")).toArray(), QJsonArray({QStringLiteral("a"), QStringLiteral("b")}));
        QCOMPARE(config.value(QStringLiteral("command_topic")).toString(), base + QStringLiteral("/set"));
        QCOMPARE(waitForMessage(base, since), QByteArray("a"));

        QVERIFY(waitSubscribed(base + QStringLiteral("/set")));
        QSignalSpy selected(&select, &Select::optionSelected);
        const qsizetype beforeCommands = mark();
        send(base + QStringLiteral("/set"), "b");
        send(base + QStringLiteral("/set"), "not-an-option");
        send(base + QStringLiteral("/set"), "a");
        QTRY_VERIFY(!selected.isEmpty() && selected.last().at(0).toString() == QLatin1String("a"));
        QCOMPARE(selected.first().at(0).toString(), QStringLiteral("b"));
        QCOMPARE(select.state(), QStringLiteral("a"));
        // the select reports each command back; wait for the last report to arrive
        QTRY_VERIFY(!messagesOn(base, beforeCommands).isEmpty() && messagesOn(base, beforeCommands).last() == "a");

        // Home Assistant rejects a state that is not one of the options ("Invalid option for ...")
        QEXPECT_FAIL("", "Select passes on and publishes any command payload, not only its options", Continue);
        QCOMPARE(selected.count(), 2);
        QEXPECT_FAIL("", "Select passes on and publishes any command payload, not only its options", Continue);
        QVERIFY(!messagesOn(base, beforeCommands).contains("not-an-option"));
    }

    void testNumber()
    {
        const qsizetype since = mark();
        const QString base = baseTopic(QStringLiteral("test_number"));

        Number number;
        number.setId(QStringLiteral("test_number"));
        number.setName(QStringLiteral("Number"));
        number.setRange(0, 150, 5, QStringLiteral("%"));
        number.setValue(30);
        number.runtimeRegistration();

        const QJsonObject config = waitForJson(discoveryTopic(QStringLiteral("number"), QStringLiteral("test_number")), since);
        QCOMPARE(config.value(QStringLiteral("min")).toVariant().toInt(), 0);
        QCOMPARE(config.value(QStringLiteral("max")).toVariant().toInt(), 150);
        QCOMPARE(config.value(QStringLiteral("step")).toVariant().toInt(), 5);
        QCOMPARE(config.value(QStringLiteral("unit_of_measurement")).toString(), QStringLiteral("%"));
        QCOMPARE(waitForMessage(base, since), QByteArray("30"));

        QVERIFY(waitSubscribed(base + QStringLiteral("/set")));
        QSignalSpy requested(&number, &Number::valueChangeRequested);
        send(base + QStringLiteral("/set"), "abc");
        send(base + QStringLiteral("/set"), "42");
        QTRY_COMPARE(requested.count(), 1);
        QCOMPARE(requested.first().at(0).toInt(), 42);
    }

    void testButtonTextAndLock()
    {
        const qsizetype since = mark();

        Button button;
        button.setId(QStringLiteral("test_button"));
        button.setName(QStringLiteral("Button"));
        button.runtimeRegistration();
        const QJsonObject buttonConfig = waitForJson(discoveryTopic(QStringLiteral("button"), QStringLiteral("test_button")), since);
        QCOMPARE(buttonConfig.value(QStringLiteral("command_topic")).toString(), baseTopic(QStringLiteral("test_button")));

        Text text;
        text.setId(QStringLiteral("test_text"));
        text.setName(QStringLiteral("Text"));
        text.runtimeRegistration();
        QVERIFY(waitForMessage(discoveryTopic(QStringLiteral("text"), QStringLiteral("test_text")), since));

        Lock lock;
        lock.setId(QStringLiteral("test_lock"));
        lock.setName(QStringLiteral("Lock"));
        lock.setState(true);
        lock.runtimeRegistration();
        const QJsonObject lockConfig = waitForJson(discoveryTopic(QStringLiteral("lock"), QStringLiteral("test_lock")), since);
        QCOMPARE(lockConfig.value(QStringLiteral("payload_lock")).toString(), QStringLiteral("true"));
        QCOMPARE(lockConfig.value(QStringLiteral("state_locked")).toString(), QStringLiteral("true"));
        QCOMPARE(waitForMessage(baseTopic(QStringLiteral("test_lock")), since), QByteArray("true"));

        QVERIFY(waitSubscribed(baseTopic(QStringLiteral("test_button"))));
        QVERIFY(waitSubscribed(baseTopic(QStringLiteral("test_text")) + QStringLiteral("/set")));
        QVERIFY(waitSubscribed(baseTopic(QStringLiteral("test_lock")) + QStringLiteral("/set")));

        QSignalSpy pressed(&button, &Button::triggered);
        QSignalSpy textRequested(&text, &Text::stateChangeRequested);
        QSignalSpy lockRequested(&lock, &Lock::stateChangeRequested);
        const qsizetype beforeCommands = mark();
        send(baseTopic(QStringLiteral("test_button")), "PRESS");
        send(baseTopic(QStringLiteral("test_text")) + QStringLiteral("/set"), "hällo wörld");
        send(baseTopic(QStringLiteral("test_lock")) + QStringLiteral("/set"), "false");

        QTRY_COMPARE(pressed.count(), 1);
        QTRY_COMPARE(textRequested.count(), 1);
        QCOMPARE(textRequested.first().at(0).toString(), QStringLiteral("hällo wörld"));
        // the text entity reports the new text back right away
        QCOMPARE(waitForMessage(baseTopic(QStringLiteral("test_text")), beforeCommands), QStringLiteral("hällo wörld").toUtf8());
        QTRY_COMPARE(lockRequested.count(), 1);
        QCOMPARE(lockRequested.first().at(0).toBool(), false);
    }

    void testEvent()
    {
        const qsizetype since = mark();
        Event event;
        event.setId(QStringLiteral("test_event"));
        event.setName(QStringLiteral("Meta+X"));
        event.runtimeRegistration();

        const QJsonObject config = waitForJson(discoveryTopic(QStringLiteral("device_automation"), QStringLiteral("test_event")), since);
        QCOMPARE(config.value(QStringLiteral("automation_type")).toString(), QStringLiteral("trigger"));
        QCOMPARE(config.value(QStringLiteral("topic")).toString(), baseTopic(QStringLiteral("test_event")));
        QCOMPARE(config.value(QStringLiteral("type")).toString(), QStringLiteral("button_short_press"));
        QCOMPARE(config.value(QStringLiteral("subtype")).toString(), QStringLiteral("Meta+X"));

        const qsizetype beforeTrigger = mark();
        event.trigger();
        QTRY_COMPARE(messagesOn(baseTopic(QStringLiteral("test_event")), beforeTrigger), QList<QByteArray>({"pressed", ""}));
    }

    void testUpdate()
    {
        const qsizetype since = mark();
        const QString base = baseTopic(QStringLiteral("test_update"));
        Update update;
        update.setId(QStringLiteral("test_update"));
        update.setName(QStringLiteral("Update"));
        update.setInstalledVersion(QStringLiteral("1.0"));
        update.setLatestVersion(QStringLiteral("1.1"));
        update.runtimeRegistration();

        const QJsonObject config = waitForJson(discoveryTopic(QStringLiteral("update"), QStringLiteral("test_update")), since);
        QCOMPARE(config.value(QStringLiteral("payload_install")).toString(), QStringLiteral("install"));
        // every setter publishes right away while connected; the state of init() has both versions
        QTRY_COMPARE(messagesOn(base, since).size(), 3);
        const QJsonObject state = QJsonDocument::fromJson(messagesOn(base, since).last()).object();
        QCOMPARE(state.value(QStringLiteral("installed_version")).toString(), QStringLiteral("1.0"));
        QCOMPARE(state.value(QStringLiteral("latest_version")).toString(), QStringLiteral("1.1"));
        QVERIFY(state.value(QStringLiteral("update_percentage")).isNull());

        const qsizetype beforeProgress = mark();
        update.setInProgress(true);
        update.setUpdatePercentage(40);
        update.setUpdatePercentage(101); // out of range: ignored
        QTRY_COMPARE(messagesOn(base, beforeProgress).size(), 2);
        QCOMPARE(QJsonDocument::fromJson(messagesOn(base, beforeProgress).last()).object().value(QStringLiteral("update_percentage")).toInt(), 40);
        QCOMPARE(update.updatePercentage(), 40);

        QVERIFY(waitSubscribed(base + QStringLiteral("/set")));
        QSignalSpy install(&update, &Update::installRequested);
        send(base + QStringLiteral("/set"), "something-else");
        send(base + QStringLiteral("/set"), "install");
        QTRY_COMPARE(install.count(), 1);
    }

    void testNotifyAndCamera()
    {
        const qsizetype since = mark();
        Notify notify;
        notify.setId(QStringLiteral("test_notify"));
        notify.setName(QStringLiteral("Notify"));
        notify.runtimeRegistration();
        const QJsonObject notifyConfig = waitForJson(discoveryTopic(QStringLiteral("notify"), QStringLiteral("test_notify")), since);
        QCOMPARE(notifyConfig.value(QStringLiteral("command_topic")).toString(), baseTopic(QStringLiteral("test_notify")) + QStringLiteral("/notifications"));

        Camera camera;
        camera.setId(QStringLiteral("test_camera"));
        camera.setName(QStringLiteral("Camera"));
        camera.runtimeRegistration();
        const QJsonObject cameraConfig = waitForJson(discoveryTopic(QStringLiteral("camera"), QStringLiteral("test_camera")), since);
        QCOMPARE(cameraConfig.value(QStringLiteral("topic")).toString(), baseTopic(QStringLiteral("test_camera")));
        QCOMPARE(cameraConfig.value(QStringLiteral("image_encoding")).toString(), QStringLiteral("b64"));

        QVERIFY(waitSubscribed(baseTopic(QStringLiteral("test_notify")) + QStringLiteral("/notifications")));
        QVERIFY(waitSubscribed(baseTopic(QStringLiteral("test_camera")) + QStringLiteral("/command")));
        QSignalSpy notified(&notify, &Notify::notificationReceived);
        QSignalSpy commanded(&camera, &Camera::commandReceived);
        send(baseTopic(QStringLiteral("test_notify")) + QStringLiteral("/notifications"), R"({"title":"t","message":"m"})");
        send(baseTopic(QStringLiteral("test_camera")) + QStringLiteral("/command"), "snapshot");
        QTRY_COMPARE(notified.count(), 1);
        QCOMPARE(notified.first().at(0).toByteArray(), QByteArray(R"({"title":"t","message":"m"})"));
        QTRY_COMPARE(commanded.count(), 1);
        QCOMPARE(commanded.first().at(0).toString(), QStringLiteral("snapshot"));

        const qsizetype beforeImage = mark();
        camera.publishImage("aGVsbG8=");
        QCOMPARE(waitForMessage(baseTopic(QStringLiteral("test_camera")), beforeImage), QByteArray("aGVsbG8="));
        const QJsonObject attributes = waitForJson(baseTopic(QStringLiteral("test_camera")) + QStringLiteral("/attributes"), beforeImage);
        QCOMPARE(attributes.value(QStringLiteral("size_bytes")).toInt(), 8);
    }

    void testImage()
    {
        const qsizetype since = mark();
        Image image;
        image.setId(QStringLiteral("test_image"));
        image.setName(QStringLiteral("Image"));
        image.setMimeType(QStringLiteral("image/png"));
        image.runtimeRegistration();
        const QJsonObject config = waitForJson(discoveryTopic(QStringLiteral("image"), QStringLiteral("test_image")), since);
        QCOMPARE(config.value(QStringLiteral("image_topic")).toString(), baseTopic(QStringLiteral("test_image")) + QStringLiteral("/image"));
        QCOMPARE(config.value(QStringLiteral("content_type")).toString(), QStringLiteral("image/png"));
        QVERIFY(!config.contains(QStringLiteral("url_topic")));

        Image urlImage;
        urlImage.setId(QStringLiteral("test_image_url"));
        urlImage.setName(QStringLiteral("Image URL"));
        urlImage.setIsUrlMode(true);
        urlImage.runtimeRegistration();
        const QJsonObject urlConfig = waitForJson(discoveryTopic(QStringLiteral("image"), QStringLiteral("test_image_url")), since);
        QCOMPARE(urlConfig.value(QStringLiteral("url_topic")).toString(), baseTopic(QStringLiteral("test_image_url")) + QStringLiteral("/url"));
        // image_topic and url_topic are exclusive in Home Assistant's schema, which rejects the config
        QEXPECT_FAIL("", "Image::init() lacks an else: URL mode also sets image_topic and content_type", Continue);
        QVERIFY(!urlConfig.contains(QStringLiteral("image_topic")));
    }

    void testMediaPlayer()
    {
        const qsizetype since = mark();
        const QString base = baseTopic(QStringLiteral("test_player"));
        MediaPlayer player;
        player.setId(QStringLiteral("test_player"));
        player.setName(QStringLiteral("Player"));
        player.runtimeRegistration();

        const QJsonObject config = waitForJson(discoveryTopic(QStringLiteral("media_player"), QStringLiteral("test_player")), since);
        QCOMPARE(config.value(QStringLiteral("command_play_topic")).toString(), base + QStringLiteral("/play"));
        QCOMPARE(config.value(QStringLiteral("command_volume_topic")).toString(), base + QStringLiteral("/set_volume"));
        // the play/pause toggle is sent to the topic the player publishes its media type on
        QEXPECT_FAIL("", "MediaPlayer::init() sets command_playpause_topic to .../mediatype", Continue);
        QVERIFY(config.value(QStringLiteral("command_playpause_topic")).toString() != config.value(QStringLiteral("state_mediatype_topic")).toString());

        QVERIFY(waitSubscribed(base + QStringLiteral("/play")));
        QVERIFY(waitSubscribed(base + QStringLiteral("/set_volume")));
        QVERIFY(waitSubscribed(base + QStringLiteral("/setposition")));
        QSignalSpy play(&player, &MediaPlayer::playRequested);
        QSignalSpy volume(&player, &MediaPlayer::volumeChanged);
        QSignalSpy position(&player, &MediaPlayer::positionChanged);
        const qsizetype beforeCommands = mark();
        send(base + QStringLiteral("/play"), "");
        send(base + QStringLiteral("/set_volume"), "0.25");
        send(base + QStringLiteral("/setposition"), "1.5");
        QTRY_COMPARE(play.count(), 1);
        QCOMPARE(waitForMessage(base + QStringLiteral("/state"), beforeCommands), QByteArray("playing"));
        QTRY_COMPARE(volume.count(), 1);
        QCOMPARE(volume.first().at(0).toDouble(), 0.25);
        QTRY_COMPARE(position.count(), 1);
        QCOMPARE(position.first().at(0).toLongLong(), 1500000); // seconds from HA, microseconds for MPRIS

        const qsizetype beforeState = mark();
        player.setState({{QStringLiteral("state"), QStringLiteral("Paused")}, {QStringLiteral("title"), QStringLiteral("Song")}});
        QCOMPARE(waitForMessage(base + QStringLiteral("/state"), beforeState), QByteArray("paused"));
        QCOMPARE(waitForMessage(base + QStringLiteral("/title"), beforeState), QByteArray("Song"));
    }

    void testUnregister()
    {
        const qsizetype since = mark();
        Sensor sensor;
        sensor.setId(QStringLiteral("test_unregister"));
        sensor.setName(QStringLiteral("Gone"));
        sensor.runtimeRegistration();
        const QString topic = discoveryTopic(QStringLiteral("sensor"), QStringLiteral("test_unregister"));
        QVERIFY(waitForMessage(topic, since));

        const qsizetype beforeUnregister = mark();
        sensor.unRegister();
        // an empty retained config removes the entity from Home Assistant
        QCOMPARE(waitForMessage(topic, beforeUnregister), QByteArray());
    }

    void testRuntimeRegistrationTwice()
    {
        // Audio re-registers its output volume each time ~/.config/plasmaparc changes, Bluetooth a
        // device's switch each time it comes back
        const QString base = baseTopic(QStringLiteral("test_twice"));
        Switch sw;
        sw.setId(QStringLiteral("test_twice"));
        sw.setName(QStringLiteral("Twice"));
        sw.runtimeRegistration();
        sw.runtimeRegistration();
        QVERIFY(waitSubscribed(base + QStringLiteral("/set")));

        QSignalSpy requested(&sw, &Switch::stateChangeRequested);
        send(base + QStringLiteral("/set"), "true");
        send(base + QStringLiteral("/set"), "false");
        QTRY_VERIFY(!requested.isEmpty() && requested.last().at(0).toBool() == false);
        QEXPECT_FAIL("", "init() connects the command handler again to the subscription QtMqtt hands back", Continue);
        QCOMPARE(requested.count(), 2);
    }

    void testReconnect()
    {
        const QString base = baseTopic(QStringLiteral("test_reconnect"));
        Switch sw;
        sw.setId(QStringLiteral("test_reconnect"));
        sw.setName(QStringLiteral("Reconnect"));
        sw.runtimeRegistration();
        QVERIFY(waitSubscribed(base + QStringLiteral("/set")));

        m_transport->doDisconnect();
        QTRY_COMPARE(TransportManager::mqttClient()->state(), QMqttClient::Disconnected);
        const qsizetype since = mark();
        m_transport->doConnect();
        QVERIFY(waitConnected());

        // every entity registers again on connect
        QVERIFY(waitForMessage(discoveryTopic(QStringLiteral("switch"), QStringLiteral("test_reconnect")), since));
        QVERIFY(waitSubscribed(base + QStringLiteral("/set")));
        QSignalSpy requested(&sw, &Switch::stateChangeRequested);
        send(base + QStringLiteral("/set"), "true");
        send(base + QStringLiteral("/set"), "false");
        QTRY_VERIFY(!requested.isEmpty() && requested.last().at(0).toBool() == false);
        QCOMPARE(requested.count(), 2);
    }

    void testBrokerRestart()
    {
        const QString base = baseTopic(QStringLiteral("test_restart"));
        Switch sw;
        sw.setId(QStringLiteral("test_restart"));
        sw.setName(QStringLiteral("Restart"));
        sw.setState(true);
        sw.runtimeRegistration();
        QVERIFY(waitSubscribed(base + QStringLiteral("/set")));

        m_broker.stop();
        QTRY_COMPARE(TransportManager::mqttClient()->state(), QMqttClient::Disconnected);
        QTRY_COMPARE(m_observer->state(), QMqttClient::Disconnected);
        QVERIFY(m_broker.start());
        QVERIFY(connectObserver());
        const qsizetype since = mark();

        // the broker lost every retained message; kiot reconnects by itself (every 5 s) and
        // publishes discovery and state again
        QVERIFY(waitConnected(15000));
        QVERIFY(waitForMessage(discoveryTopic(QStringLiteral("switch"), QStringLiteral("test_restart")), since));
        QCOMPARE(waitForMessage(base, since), QByteArray("true"));

        QVERIFY(waitSubscribed(base + QStringLiteral("/set")));
        QSignalSpy requested(&sw, &Switch::stateChangeRequested);
        send(base + QStringLiteral("/set"), "false");
        QTRY_COMPARE(requested.count(), 1);
    }

    void cleanupTestCase()
    {
        if (m_transport) {
            m_transport->doDisconnect();
        }
        m_broker.stop();
    }
};

QTEST_GUILESS_MAIN(MqttEntitiesTest)

#include "mqttentitiestest.moc"
