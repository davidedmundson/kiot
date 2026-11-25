// SPDX-FileCopyrightText: 2026 Kloud <dgudim@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "customsensor.h"

#include <QTimer>
#include <QHash>
#include <QStringView>
#include <KProcess>
#include <KSandbox>
#include <KIOTShared/kiotshared.h>
#include <algorithm>
#include <cmath>
#include <limits>
using KIOTShared::PlatformHelper;
DEFINE_PLUGIN_LOGGER(customSensors,CustomSensors)

constexpr qint64 MinimumIntervalMs = 1000;
// QTimer takes the interval as an int: about 24.8 days
constexpr qint64 MaximumIntervalMs = std::numeric_limits<int>::max();
// A command still running after max(interval, this) is killed
constexpr int MinimumCommandTimeoutMs = 5 * 60 * 1000;

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

// A span qint64 can't hold (or inf/nan) counts as unparsable; casting it would be undefined
static qint64 toMs(double ms)
{
    if (!(std::abs(ms) < 9.0e18)) {
        return -1;
    }
    return (qint64)ms;
}

qint64 parseTimeSpanToMs(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }

    bool parsedAsBareNumber = false;
    const double bareSeconds = trimmed.toDouble(&parsedAsBareNumber);
    if (parsedAsBareNumber) {
        return toMs(bareSeconds * 1000.0);
    }

    double totalMs = 0.0;
    for (const QString &token : trimmed.simplified().split(u' ', Qt::SkipEmptyParts)) {
        const auto unitStart = std::find_if(token.cbegin(), token.cend(), [](QChar c) {
            return !c.isDigit() && c != u'.';
        });
        const auto splitAt = unitStart - token.cbegin();
        if (splitAt == 0 || splitAt == token.size()) {
            return -1;
        }

        bool numberParsedSuccessfully = false;
        const double value = QStringView(token).first(splitAt).toDouble(&numberParsedSuccessfully);
        const auto unitMsMultiplier = unitToMs.constFind(token.sliced(splitAt));
        if (!numberParsedSuccessfully || unitMsMultiplier == unitToMs.constEnd()) {
            return -1;
        }
        totalMs += value * unitMsMultiplier.value();
    }

    return toMs(totalMs);
}

CustomSensor::CustomSensor(const QString &id, const QString &name, const QString &command, qint64 intervalMs, QObject *parent)
    : QObject(parent)
    , m_command(command)
{
    m_sensor = new Sensor(this);
    m_sensor->setId(id);
    m_sensor->setName(name);

    m_timer = new QTimer(this);
    if (intervalMs > MaximumIntervalMs) {
        qCWarning(customSensors) << "Interval of custom sensor" << id << "is longer than" << MaximumIntervalMs << "ms, using that";
    }
    m_timer->setInterval(int(std::clamp(intervalMs, MinimumIntervalMs, MaximumIntervalMs)));
    connect(m_timer, &QTimer::timeout, this, &CustomSensor::poll);
    m_timer->start();

    poll();
}

Sensor *CustomSensor::sensor() const
{
    return m_sensor;
}

void CustomSensor::poll()
{
    if (m_process) {
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
            qCWarning(customSensors) << "Command failed with exit code" << exitCode;
        } else {
            const QString output = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
            m_sensor->setState(output);
        }
        m_process->deleteLater();
        m_process = nullptr;
    });
    // finished() never comes for a command that can't be started
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        qCWarning(customSensors) << "Command failed to start:" << m_process->errorString();
        m_process->deleteLater();
        m_process = nullptr;
    });
    // A command that hangs is killed, so it can't stop the sensor for good; finished() then
    // clears m_process and the next poll starts it again
    QTimer::singleShot(std::max(m_timer->interval(), MinimumCommandTimeoutMs), m_process, &QProcess::kill);

    m_process->start();
}