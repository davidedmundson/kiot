// SPDX-FileCopyrightText: 2026 Thomas Kastenholz <2893148+Thokas@users.noreply.github.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "suspendblockers.h"

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
QList<SuspendBlockers::Inhibition> parseActiveInhibitions(const QDBusArgument &arg)
{
    QList<SuspendBlockers::Inhibition> result;
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
QList<SuspendBlockers::Inhibition> parseLegacyInhibitions(const QDBusArgument &arg)
{
    QList<SuspendBlockers::Inhibition> result;
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

bool blocksSleep(const SuspendBlockers::Inhibition &inhibition)
{
    // the legacy API doesn't tell, so there every inhibition counts
    return inhibition.what.isEmpty() || inhibition.what.split(QLatin1Char(':')).contains(QLatin1String("sleep"));
}

QVariantMap toVariant(const SuspendBlockers::Inhibition &inhibition)
{
    QVariantMap map{{QStringLiteral("who"), inhibition.who}, {QStringLiteral("why"), inhibition.why}};
    if (!inhibition.what.isEmpty()) {
        map.insert(QStringLiteral("what"), inhibition.what);
    }
    return map;
}
}

SuspendBlockers::SuspendBlockers(QObject *parent)
    : QObject(parent)
    , m_sensor(new Sensor(this))
    , m_watcher(new QDBusServiceWatcher(PowerDevilService,
                                        QDBusConnection::sessionBus(),
                                        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                        this))
{
    m_sensor->setId(QStringLiteral("suspend_blockers"));
    m_sensor->setName(QStringLiteral("Suspend blockers"));
    m_sensor->setHaIcon(QStringLiteral("mdi:sleep-off"));
    m_sensor->setDiscoveryConfig(QStringLiteral("state_class"), QStringLiteral("measurement"));

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
        qCInfo(plugin_logger) << "PowerDevil left the session bus, suspend blockers unknown";
        setUnknown();
    });

    setUnknown();
    m_sensor->runtimeRegistration(); // no-op before the MQTT connection, registers when started at runtime
    refresh();
}

SuspendBlockers::~SuspendBlockers() = default;

void SuspendBlockers::onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    // PowerDevil only names the changed properties (invalidated), without values
    if (interface == PolicyAgentInterface && (changed.contains(ActiveInhibitions) || invalidated.contains(ActiveInhibitions))) {
        refresh();
    }
}

void SuspendBlockers::onLegacyInhibitionsChanged()
{
    if (m_legacy) {
        refreshLegacy();
    }
}

void SuspendBlockers::refresh()
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

void SuspendBlockers::refreshLegacy()
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

void SuspendBlockers::setUnknown()
{
    // "None" = unknown in Home Assistant (an empty payload would be ignored and keep the old count)
    m_published = false;
    m_last.clear();
    m_sensor->setAttributes({{QStringLiteral("summary"), QStringLiteral("unknown (PowerDevil not available)")}});
    m_sensor->setState(QStringLiteral("None"));
}

void SuspendBlockers::publish(const QList<Inhibition> &inhibitions, const QString &source)
{
    if (m_published && inhibitions == m_last) {
        return;
    }
    m_published = true;
    m_last = inhibitions;

    QVariantList blockers, screen;
    QStringList summary;
    for (const auto &inhibition : inhibitions) {
        if (blocksSleep(inhibition)) {
            blockers.append(toVariant(inhibition));
            summary.append(inhibition.why.isEmpty() ? inhibition.who : inhibition.who + QStringLiteral(": ") + inhibition.why);
        } else {
            screen.append(toVariant(inhibition));
        }
    }
    qCDebug(plugin_logger) << "Suspend blockers:" << (summary.isEmpty() ? QStringLiteral("none") : summary.join(QStringLiteral("; ")));
    m_sensor->setAttributes({
        {QStringLiteral("summary"), summary.isEmpty() ? QStringLiteral("none") : summary.join(QStringLiteral("; "))},
        {QStringLiteral("blockers"), blockers},
        {QStringLiteral("screen_inhibitions"), screen},
        {QStringLiteral("source"), source},
    });
    m_sensor->setState(QString::number(blockers.size()));
}
