// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

// The parts of Entity that need no broker: ids, topics, icons and the conversion of attribute
// values. Everything that publishes is in mqttentitiestest.

#include <QHostInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTest>
#include <QTimeZone>

#include "entities/entity.h"
#include "testenv.h"

using KIOTShared::Entities::Entity;

static KiotTest::IsolatedEnvironment s_env;

namespace
{
class TestEntity : public Entity
{
public:
    TestEntity()
        : Entity(nullptr)
    {
    }
    using Entity::convertForHomeAssistant;
};
} // namespace

class EntityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // read once and cached by Entity, so set before the first entity asks
        KiotTest::writeGeneralConfig({{QStringLiteral("host"), QStringLiteral("localhost")},
                                      {QStringLiteral("topicprefix"), QStringLiteral("desk")},
                                      {QStringLiteral("discoveryprefix"), QStringLiteral("ha")}});
    }

    void testPrefixesFromConfig()
    {
        QCOMPARE(Entity::topixPrefix(), QStringLiteral("desk"));
        QCOMPARE(Entity::discoveryPrefix(), QStringLiteral("ha"));
    }

    void testIdSanitizing_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("expected");
        QTest::newRow("plain") << QStringLiteral("output_volume") << QStringLiteral("output_volume");
        QTest::newRow("spaces and case") << QStringLiteral("Output Volume") << QStringLiteral("output_volume");
        QTest::newRow("punctuation") << QStringLiteral("docker_my.app:latest") << QStringLiteral("docker_my_app_latest");
        QTest::newRow("runs and edges") << QStringLiteral("__a  //  b__") << QStringLiteral("a_b");
        QTest::newRow("dash kept") << QStringLiteral("game-123") << QStringLiteral("game-123");
        QTest::newRow("mqtt wildcards") << QStringLiteral("a/+/#") << QStringLiteral("a");
        QTest::newRow("empty") << QString() << QStringLiteral("empty_entity_id_detected_integration_error");
        QTest::newRow("only symbols") << QStringLiteral("!!!") << QStringLiteral("empty_entity_id_detected_integration_error");
    }

    void testIdSanitizing()
    {
        QFETCH(QString, id);
        QFETCH(QString, expected);
        TestEntity entity;
        entity.setId(id);
        QCOMPARE(entity.id(), expected);
    }

    void testIdIsValidForHomeAssistantDiscovery_data()
    {
        QTest::addColumn<QString>("id");
        QTest::newRow("umlaut") << QStringLiteral("Kopfhörer");
        QTest::newRow("accent") << QStringLiteral("Café Script");
        QTest::newRow("cjk") << QStringLiteral("游戏");
    }

    void testIdIsValidForHomeAssistantDiscovery()
    {
        // Home Assistant ignores a discovery topic whose object id is not [a-zA-Z0-9_-]+
        // (TOPIC_MATCHER in homeassistant/components/mqtt/discovery.py), so such an entity never appears.
        QFETCH(QString, id);
        TestEntity entity;
        entity.setId(id);
        static const QRegularExpression valid(QStringLiteral("^[a-zA-Z0-9_-]+$"));
        QEXPECT_FAIL("", "sanitizeForMqttTopic() keeps non-ASCII letters (QChar::isLetterOrNumber)", Continue);
        QVERIFY2(valid.match(entity.id()).hasMatch(), qPrintable(entity.id()));
    }

    void testTopics()
    {
        TestEntity entity;
        entity.setId(QStringLiteral("Night Mode"));
        QCOMPARE(entity.hostname(), QHostInfo::localHostName().toLower());
        QCOMPARE(entity.baseTopic(), QStringLiteral("desk/%1/night_mode").arg(entity.hostname()));
    }

    void testIcon()
    {
        TestEntity entity;
        QVERIFY(entity.haIcon().isEmpty());
        entity.setDiscoveryConfig(QStringLiteral("icon"), QStringLiteral("mdi:lightbulb"));
        QCOMPARE(entity.haIcon(), QStringLiteral("mdi:lightbulb"));
        entity.setHaIcon(QStringLiteral("mdi:lamp"));
        QCOMPARE(entity.haIcon(), QStringLiteral("mdi:lamp"));
    }

    void testConvertForHomeAssistant()
    {
        TestEntity entity;

        QCOMPARE(entity.convertForHomeAssistant(true), QVariant(QStringLiteral("true")));
        QCOMPARE(entity.convertForHomeAssistant(false), QVariant(QStringLiteral("false")));
        QCOMPARE(entity.convertForHomeAssistant(42), QVariant(42));
        QCOMPARE(entity.convertForHomeAssistant(QStringLiteral("text")), QVariant(QStringLiteral("text")));

        const QDateTime when(QDate(2025, 1, 15), QTime(10, 30), QTimeZone::UTC);
        QCOMPARE(entity.convertForHomeAssistant(when), QVariant(QStringLiteral("2025-01-15T10:30:00Z")));

        const QVariant list = entity.convertForHomeAssistant(QVariantList{1, QStringLiteral("two"), true});
        QCOMPARE(list.value<QJsonArray>(), QJsonArray({1, QStringLiteral("two"), true}));

        const QVariant map = entity.convertForHomeAssistant(QVariantMap{{QStringLiteral("a"), 1}, {QStringLiteral("b"), QStringLiteral("x")}});
        QCOMPARE(map.value<QJsonObject>(), QJsonObject({{QStringLiteral("a"), 1}, {QStringLiteral("b"), QStringLiteral("x")}}));
    }
};

QTEST_GUILESS_MAIN(EntityTest)

#include "entitytest.moc"
