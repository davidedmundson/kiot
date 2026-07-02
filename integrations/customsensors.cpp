// SPDX-FileCopyrightText: 2026 Kloud <dgudim@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/entities.h"

#include <KConfigGroup>
#include <KProcess>
#include <KSandbox>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QTimer>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(customSensors)
Q_LOGGING_CATEGORY(customSensors, "integration.CustomSensors")

constexpr qint64 MinimumIntervalMs = 1000;
constexpr qint64 DefaultIntervalMs = 10 * 1000;

static const QHash<QString, double> unitToMs = {
    {QStringLiteral("s"), 1000.0},
    {QStringLiteral("sec"), 1000.0},
    {QStringLiteral("second"), 1000.0},
    {QStringLiteral("seconds"), 1000.0},
    {QStringLiteral("m"), 60000.0},
    {QStringLiteral("min"), 60000.0},
    {QStringLiteral("minute"), 60000.0},
    {QStringLiteral("minutes"), 60000.0},
    {QStringLiteral("h"), 3600000.0},
    {QStringLiteral("hr"), 3600000.0},
    {QStringLiteral("hour"), 3600000.0},
    {QStringLiteral("hours"), 3600000.0},
    {QStringLiteral("d"), 86400000.0},
    {QStringLiteral("day"), 86400000.0},
    {QStringLiteral("days"), 86400000.0},
};

// Parse a systemd.time style time span ("1m 30s", "2h").
// NOTE: Some units are deliberately not supported (microseconds, milliseconds, weeks, months, years).
// NOTE: Only whitespace-delimited style is supported (e.g. "1m 30s", not "1m30s").
// A bare number without a unit is interpreted as seconds
qint64 parseTimeSpanToMs(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }

    bool parsedAsBareNumber = false;
    const double bareSeconds = trimmed.toDouble(&parsedAsBareNumber);
    if (parsedAsBareNumber) {
        return (qint64)(bareSeconds * 1000.0);
    }

    // Each whitespace-separated token is a number directly followed by a unit,
    // e.g. "1m 30s".
    double totalMs = 0.0;
    for (const QString &token : trimmed.simplified().split(u' ', Qt::SkipEmptyParts)) {
        const auto unitStart = std::find_if(token.cbegin(), token.cend(), [](QChar c) {
            return !c.isDigit() && c != u'.';
        });
        const auto splitAt = unitStart - token.cbegin();
        if (splitAt == 0 || splitAt == token.size()) {
            return -1; // Missing number or missing unit
        }

        bool numberParsedSuccessfully = false;
        const double value = QStringView(token).first(splitAt).toDouble(&numberParsedSuccessfully);
        const auto unitMsMultiplier = unitToMs.constFind(token.sliced(splitAt));
        if (!numberParsedSuccessfully || unitMsMultiplier == unitToMs.constEnd()) {
            return -1; // Invalid number or unknown unit
        }
        totalMs += value * unitMsMultiplier.value();
    }

    return (qint64)totalMs;
}

class CustomSensor : public QObject
{
    Q_OBJECT
public:
    CustomSensor(const QString &id, const QString &name, const QString &command, qint64 intervalMs, QObject *parent)
        : QObject(parent)
        , m_command(command)
    {
        m_sensor = new Sensor(this);
        m_sensor->setId(id);
        m_sensor->setName(name);

        m_timer = new QTimer(this);
        m_timer->setInterval(std::max(intervalMs, MinimumIntervalMs));
        connect(m_timer, &QTimer::timeout, this, &CustomSensor::poll);
        m_timer->start();

        // Publish initial value, don't wait for the first interval
        poll();
    }

    Sensor *sensor() const
    {
        return m_sensor;
    }

private:
    void poll()
    {
        if (m_process) {
            qCDebug(customSensors) << "Skipping poll for" << m_sensor->id() << "- previous command still running";
            return;
        }

        m_process = new KProcess(this);
        m_process->setOutputChannelMode(KProcess::OnlyStdoutChannel);
        m_process->setShellCommand(m_command);

        if (KSandbox::isFlatpak()) {
            KSandbox::ProcessContext ctx = KSandbox::makeHostContext(*m_process);
            m_process->setProgram(ctx.program);
            m_process->setArguments(ctx.arguments);
        }

        connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                qCWarning(customSensors) << "Command for" << m_sensor->id() << "failed (exit" << exitCode << "):" << m_process->readAllStandardError();
            } else {
                const QString output = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
                m_sensor->setState(output);
            }
            m_process->deleteLater();
            m_process = nullptr;
        });

        m_process->start();
    }

    Sensor *m_sensor;
    QString m_command;
    QTimer *m_timer;
    KProcess *m_process = nullptr;
};

void registerCustomSensors()
{
    auto sensorConfigToplevel = KSharedConfig::openConfig()->group("CustomSensors");
    const QStringList sensorIds = sensorConfigToplevel.groupList();
    int loaded = 0;
    for (const QString &sensorId : sensorIds) {
        const KConfigGroup group = sensorConfigToplevel.group(sensorId);

        const QString command = group.readEntry("command");
        if (command.isEmpty()) {
            qCWarning(customSensors) << "Skipping custom sensor '" << sensorId << "'. Missing command";
            continue;
        }

        const QString name = group.readEntry("name", sensorId);

        qint64 intervalMs = DefaultIntervalMs;
        const QString intervalStr = group.readEntry("interval");
        if (!intervalStr.isEmpty()) {
            const qint64 parsedMs = parseTimeSpanToMs(intervalStr);
            if (parsedMs > 0) {
                intervalMs = parsedMs;
            } else {
                qCWarning(customSensors) << "Failed to parse interval '" << intervalStr << "' for custom sensor '" << sensorId << "'. Using default";
            }
        }

        auto customSensor = new CustomSensor(sensorId, name, command, intervalMs, qApp);
        Sensor *sensor = customSensor->sensor();

        const QString deviceClass = group.readEntry("device_class");
        if (!deviceClass.isEmpty()) {
            sensor->setDiscoveryConfig("device_class", deviceClass);
        }
        const QString unit = group.readEntry("unit_of_measurement");
        if (!unit.isEmpty()) {
            sensor->setDiscoveryConfig("unit_of_measurement", unit);
        }
        const QString stateClass = group.readEntry("state_class");
        if (!stateClass.isEmpty()) {
            sensor->setDiscoveryConfig("state_class", stateClass);
        }
        const QString icon = group.readEntry("icon");
        if (!icon.isEmpty()) {
            sensor->setDiscoveryConfig("icon", icon);
        }

        loaded++;
    }

    if (loaded >= 1) {
        qCInfo(customSensors) << "Loaded" << loaded << "custom sensor(s):" << sensorIds.join(", ");
    }
}

REGISTER_INTEGRATION("CustomSensors", registerCustomSensors, true)

#include "customsensors.moc"
