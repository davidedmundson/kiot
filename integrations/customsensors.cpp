// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/entities.h"

#include <KConfigGroup>
#include <KProcess>
#include <KSandbox>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QString>
#include <QTimer>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(customSensors)
Q_LOGGING_CATEGORY(customSensors, "integration.CustomSensors")

namespace
{
constexpr int MinimumIntervalSec = 1;
constexpr int DefaultIntervalSec = 10;
}

class CustomSensor : public QObject
{
    Q_OBJECT
public:
    CustomSensor(const QString &id, const QString &name, const QString &command, int intervalSec, QObject *parent)
        : QObject(parent)
        , m_command(command)
    {
        m_sensor = new Sensor(this);
        m_sensor->setId(id);
        m_sensor->setName(name);

        m_timer = new QTimer(this);
        m_timer->setInterval(std::max(intervalSec, MinimumIntervalSec) * 1000);
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
            qCWarning(customSensors) << "Skipping custom sensor" << sensorId << "- missing command";
            continue;
        }

        const QString name = group.readEntry("name", sensorId);
        const int intervalSec = group.readEntry("every_sec", DefaultIntervalSec);

        auto customSensor = new CustomSensor(sensorId, name, command, intervalSec, qApp);
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
