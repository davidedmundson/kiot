// SPDX-FileCopyrightText: 2026 Thomas Kastenholz <2893148+Thokas@users.noreply.github.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "powerinhibitions.h"

#include <KIOTShared/kiotshared.h>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QLoggingCategory>

using KIOTShared::Entities::Sensor;

DECLARE_LOGGER(plugin_logger);

namespace
{
const QString PowerDevilService = QStringLiteral("org.kde.Solid.PowerManagement");
const QString PolicyAgentPath = QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent");
const QString PolicyAgentInterface = QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent");
const QString PropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString ActiveInhibitions = QStringLiteral("ActiveInhibitions");

// a(ssssu): what, who, why, mode, flags (PowerDevil's PolicyAgentInhibition)
QList<PowerInhibitions::Inhibition> parseActiveInhibitions(const QDBusArgument &arg)
{
    QList<PowerInhibitions::Inhibition> result;
    arg.beginArray();
    while (!arg.atEnd()) {
        QString what, who, why, mode;
        uint flags = 0;
        arg.beginStructure();
        arg >> what >> who >> why >> mode >> flags;
        arg.endStructure();
        result.append({what, who, why});
    }
    arg.endArray();
    return result;
}

// The deprecated ListInhibitions: (who, why) pairs, as aas (what PowerDevil sends) or a{ss} (what it declares)
QList<PowerInhibitions::Inhibition> parseLegacyInhibitions(const QDBusArgument &arg)
{
    QList<PowerInhibitions::Inhibition> result;
    if (arg.currentType() == QDBusArgument::MapType) {
        arg.beginMap();
        while (!arg.atEnd()) {
            QString who, why;
            arg.beginMapEntry();
            arg >> who >> why;
            arg.endMapEntry();
            result.append({QString(), who, why});
        }
        arg.endMap();
    } else {
        arg.beginArray();
        while (!arg.atEnd()) {
            QStringList pair;
            arg >> pair;
            result.append({QString(), pair.value(0), pair.value(1)});
        }
        arg.endArray();
    }
    return result;
}

// "sleep" or "idle" in what; the legacy API doesn't tell, so there every inhibition counts
bool covers(const PowerInhibitions::Inhibition &inhibition, QLatin1String part)
{
    return inhibition.what.isEmpty() || inhibition.what.split(QLatin1Char(':')).contains(part);
}

QVariantMap toVariant(const PowerInhibitions::Inhibition &inhibition)
{
    QVariantMap map{{QStringLiteral("who"), inhibition.who}, {QStringLiteral("why"), inhibition.why}};
    if (!inhibition.what.isEmpty()) {
        map.insert(QStringLiteral("what"), inhibition.what);
    }
    return map;
}

Sensor *makeSensor(QObject *parent, const QString &id, const QString &name, const QString &icon)
{
    auto sensor = new Sensor(parent);
    sensor->setId(id);
    sensor->setName(name);
    sensor->setHaIcon(icon);
    sensor->setDiscoveryConfig(QStringLiteral("state_class"), QStringLiteral("measurement"));
    return sensor;
}

// state + attributes of one sensor for the inhibitions covering part
void publishSensor(Sensor *sensor, const QList<PowerInhibitions::Inhibition> &inhibitions, QLatin1String part, const QString &source)
{
    QVariantList list;
    QStringList summary;
    for (const auto &inhibition : inhibitions) {
        if (covers(inhibition, part)) {
            list.append(toVariant(inhibition));
            summary.append(inhibition.why.isEmpty() ? inhibition.who : inhibition.who + QStringLiteral(": ") + inhibition.why);
        }
    }
    sensor->setAttributes({
        {QStringLiteral("summary"), summary.isEmpty() ? QStringLiteral("none") : summary.join(QStringLiteral("; "))},
        {QStringLiteral("inhibitions"), list},
        {QStringLiteral("source"), source},
    });
    sensor->setState(QString::number(list.size()));
}
}

PowerInhibitions::PowerInhibitions(QObject *parent)
    : QObject(parent)
    , m_suspendSensor(makeSensor(this, QStringLiteral("suspend_blockers"), QStringLiteral("Suspend blockers"), QStringLiteral("mdi:sleep-off")))
    , m_screenSensor(makeSensor(this, QStringLiteral("screen_blockers"), QStringLiteral("Screen blockers"), QStringLiteral("mdi:monitor-eye")))
    , m_watcher(new QDBusServiceWatcher(PowerDevilService,
                                        QDBusConnection::sessionBus(),
                                        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                        this))
{
    auto bus = QDBusConnection::sessionBus();
    bus.connect(PowerDevilService,
                PolicyAgentPath,
                PropertiesInterface,
                QStringLiteral("PropertiesChanged"),
                this,
                SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    bus.connect(PowerDevilService, PolicyAgentPath, PolicyAgentInterface, QStringLiteral("InhibitionsChanged"), this, SLOT(onLegacyInhibitionsChanged()));

    // PowerDevil restarts (or starts after kiot): read everything again, maybe from a different version
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        m_legacy = false;
        refresh();
    });
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        qCInfo(plugin_logger) << "PowerDevil left the session bus, suspend/screen blockers unknown";
        setUnknown();
    });

    setUnknown();
    // no-op before the MQTT connection, registers when the plugin is started at runtime
    m_suspendSensor->runtimeRegistration();
    m_screenSensor->runtimeRegistration();
    refresh();
}

PowerInhibitions::~PowerInhibitions() = default;

void PowerInhibitions::onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    // PowerDevil only names the changed properties (invalidated), without values
    if (interface == PolicyAgentInterface && (changed.contains(ActiveInhibitions) || invalidated.contains(ActiveInhibitions))) {
        refresh();
    }
}

void PowerInhibitions::onLegacyInhibitionsChanged()
{
    if (m_legacy) {
        refreshLegacy();
    }
}

void PowerInhibitions::refresh()
{
    if (m_legacy) {
        refreshLegacy();
        return;
    }
    auto message = QDBusMessage::createMethodCall(PowerDevilService, PolicyAgentPath, PropertiesInterface, QStringLiteral("Get"));
    message << PolicyAgentInterface << ActiveInhibitions;
    const quint64 request = ++m_request;
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, request](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        if (request != m_request) {
            return;
        }
        const QDBusPendingReply<QDBusVariant> reply = *call;
        if (reply.isError()) {
            const QString error = reply.error().name();
            if (error == QLatin1String("org.freedesktop.DBus.Error.InvalidArgs") || error == QLatin1String("org.freedesktop.DBus.Error.UnknownProperty")) {
                qCInfo(plugin_logger) << "PowerDevil has no ActiveInhibitions (Plasma < 6.5), using the deprecated ListInhibitions";
                m_legacy = true;
                refreshLegacy();
            } else {
                qCDebug(plugin_logger) << "Cannot read PowerDevil's inhibitions:" << reply.error().message();
                setUnknown();
            }
            return;
        }
        publish(parseActiveInhibitions(reply.value().variant().value<QDBusArgument>()), QStringLiteral("powerdevil"));
    });
}

void PowerInhibitions::refreshLegacy()
{
    auto message = QDBusMessage::createMethodCall(PowerDevilService, PolicyAgentPath, PolicyAgentInterface, QStringLiteral("ListInhibitions"));
    const quint64 request = ++m_request;
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, request](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        if (request != m_request) {
            return;
        }
        const QDBusMessage reply = call->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            qCDebug(plugin_logger) << "Cannot read PowerDevil's inhibitions (legacy):" << call->error().message();
            setUnknown();
            return;
        }
        publish(parseLegacyInhibitions(reply.arguments().at(0).value<QDBusArgument>()), QStringLiteral("powerdevil (legacy API)"));
    });
}

void PowerInhibitions::setUnknown()
{
    // "None" = unknown in Home Assistant (an empty payload would be ignored and keep the old count)
    m_published = false;
    m_last.clear();
    for (auto sensor : {m_suspendSensor, m_screenSensor}) {
        sensor->setAttributes({{QStringLiteral("summary"), QStringLiteral("unknown (PowerDevil not available)")}});
        sensor->setState(QStringLiteral("None"));
    }
}

void PowerInhibitions::publish(const QList<Inhibition> &inhibitions, const QString &source)
{
    if (m_published && inhibitions == m_last) {
        return;
    }
    m_published = true;
    m_last = inhibitions;
    publishSensor(m_suspendSensor, inhibitions, QLatin1String("sleep"), source);
    publishSensor(m_screenSensor, inhibitions, QLatin1String("idle"), source);
    qCDebug(plugin_logger) << "Inhibitions:" << inhibitions.size() << "from" << source;
}
