// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

// The interval= entry of a custom sensor ([CustomSensors][<id>] in kiotrc), as parseTimeSpanToMs reads it.

#include <QTest>

#include "customsensor.h"

class CustomSensorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testParseTimeSpan_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<qint64>("ms");

        QTest::newRow("bare seconds") << QStringLiteral("10") << qint64(10000);
        QTest::newRow("fraction") << QStringLiteral("1.5") << qint64(1500);
        QTest::newRow("negative") << QStringLiteral("-5") << qint64(-5000);
        QTest::newRow("unit") << QStringLiteral("600s") << qint64(600000);
        QTest::newRow("units summed") << QStringLiteral("1h 30m") << qint64(5400000);
        QTest::newRow("days") << QStringLiteral("30days") << qint64(2592000000);
        QTest::newRow("empty") << QString() << qint64(-1);
        QTest::newRow("unknown unit") << QStringLiteral("5x") << qint64(-1);
        QTest::newRow("unit only") << QStringLiteral("m") << qint64(-1);
        QTest::newRow("number without unit") << QStringLiteral("1 h") << qint64(-1);
        // beyond qint64: unparsable rather than an undefined cast
        QTest::newRow("huge") << QStringLiteral("1e300") << qint64(-1);
        QTest::newRow("huge with unit") << QStringLiteral("99999999999999999999999d") << qint64(-1);
        QTest::newRow("inf") << QStringLiteral("inf") << qint64(-1);
        QTest::newRow("nan") << QStringLiteral("nan") << qint64(-1);
    }

    void testParseTimeSpan()
    {
        QFETCH(QString, text);
        QFETCH(qint64, ms);
        QCOMPARE(parseTimeSpanToMs(text), ms);
    }
};

QTEST_GUILESS_MAIN(CustomSensorTest)

#include "customsensortest.moc"
