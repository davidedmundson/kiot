// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/entities.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>
#include <QRegularExpression>
#include <QProcess>

#include <KConfigGroup>
#include <KProcess>

class SystemDWatcher : public QObject
{
    Q_OBJECT
public:
    explicit SystemDWatcher(QObject *parent = nullptr);
    ~SystemDWatcher() = default;

    bool ensureConfig();
    bool init();

private slots:
    void onUnitPropertiesChanged(const QString &interface,
                                 const QVariantMap &changedProps,
                                 const QStringList &invalidatedProps,
                                 const QDBusMessage &msg);

private:
    KSharedConfig::Ptr cfg;
    QHash<QString, Switch*> m_serviceSwitches;
    QDBusInterface *m_systemdUser = nullptr;
    QString sanitizeServiceId(const QString &svc);
    QStringList listUserServices() const;
    QString pathToUnitName(const QString &path) const;
};

namespace {
    static const QRegularExpression invalidCharRegex("[^a-zA-Z0-9]");
}

SystemDWatcher::SystemDWatcher(QObject *parent)
    : QObject(parent)
{
    cfg = KSharedConfig::openConfig("kiotrc");
    if (!ensureConfig()) {
        qWarning() << "SystemD: Failed to ensure config, aborting";
        return;
    }

    m_systemdUser = new QDBusInterface(
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        QDBusConnection::sessionBus(),
        this
    );

    if (!m_systemdUser->isValid()) {
        qWarning() << "SystemDWatcher: Failed to connect to systemd user D-Bus";
        return;
    }

    if (!init()) {
        qWarning() << "SystemD: Initialization failed";
        return;
    }
}

// Ensure SystemD integration is enabled and create config entries
bool SystemDWatcher::ensureConfig()
{
    KConfigGroup intgrp(cfg, "Integrations");
    if (!intgrp.readEntry("SystemD", false)) {
        qWarning() << "Aborting: SystemD integration disabled, should not be running";
        return false;
    }
    // TODO clean up in services no longer available to make sure config is clean
    KConfigGroup grp(cfg, "systemd");
    if (!grp.exists()) {
        for (const QString &svc : listUserServices()) {
            grp.writeEntry(svc, false); // default: disabled
        }
        cfg->sync();
    }
    return true;
}

// Initialize switches and query initial state
bool SystemDWatcher::init()
{
    KConfigGroup grp(cfg, "systemd");
    if (!grp.exists()) 
        return false;

    for (const QString &svc : listUserServices()) {
        if (!grp.hasKey(svc) || !grp.readEntry(svc, false))
            continue; // skip disabled

        auto *sw = new Switch(this);
        sw->setId("systemd_" + sanitizeServiceId(svc));
        sw->setName(svc);

        // Query initial state from D-Bus
        QDBusReply<QDBusObjectPath> unitPathReply = m_systemdUser->call("GetUnit", svc);
        if (unitPathReply.isValid()) {
            QDBusObjectPath unitPath = unitPathReply.value();

            QDBusInterface unitIface(
                "org.freedesktop.systemd1",
                unitPath.path(),
                "org.freedesktop.DBus.Properties",
                QDBusConnection::sessionBus()
            );

            QDBusReply<QVariant> stateReply = unitIface.call("Get", "org.freedesktop.systemd1.Unit", "ActiveState");
            if (stateReply.isValid()) {
                sw->setState(stateReply.value().toString() == "active");
            }

            // Listen for live property changes
            QDBusConnection::sessionBus().connect(
                "org.freedesktop.systemd1",
                unitPath.path(),
                "org.freedesktop.DBus.Properties",
                "PropertiesChanged",
                this,
                SLOT(onUnitPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage))
            );
        }

        // Connect switch to systemctl for toggling service
        connect(sw, &Switch::stateChangeRequested, this, [svc](bool state) {
            QString cmd = state ? "start" : "stop"; 
            KProcess *p = new KProcess(); 
            p->setShellCommand("systemctl --user " + cmd + " " + svc); 
            p->startDetached(); 
            qDebug() << "Toggled service" << svc << "to" << (state ? "start" : "stop");
        });

        m_serviceSwitches[svc] = sw;
    }

    return true;
}

QString SystemDWatcher::sanitizeServiceId(const QString &svc)
{
    QString id = svc;
    id.replace(invalidCharRegex, QStringLiteral("_"));
    return id;
}

// List all user services (*.service)
QStringList SystemDWatcher::listUserServices() const
{
    QStringList services;
    QProcess p;
    p.start("systemctl", {"--user", "list-unit-files", "--type=service", "--no-pager", "--no-legend"});
    p.waitForFinished(3000);
    QString output = p.readAllStandardOutput();
    for (const QString &line : output.split("\n", Qt::SkipEmptyParts)) {
        QString svc = line.section(' ', 0, 0);
        if (!svc.isEmpty())
            services.append(svc);
    }
    return services;
}

// Convert D-Bus path to proper unit name
QString SystemDWatcher::pathToUnitName(const QString &path) const
{
    QString name = path.section('/', -1);
    name.replace("_2e", ".");
    name.replace("_2d", "-");
    return name;
}

// Slot for handling live updates from systemd units
void SystemDWatcher::onUnitPropertiesChanged(const QString &interface,
                                             const QVariantMap &changedProps,
                                             const QStringList &invalidatedProps,
                                             const QDBusMessage &msg)
{
    Q_UNUSED(invalidatedProps);
    if (interface != "org.freedesktop.systemd1.Unit")
        return;

    QString unitName = pathToUnitName(msg.path());
    if (!m_serviceSwitches.contains(unitName))
        return;

    if (changedProps.contains("ActiveState")) {
        QString state = changedProps["ActiveState"].toString();
        if (m_serviceSwitches[unitName]->state() != (state == "active")) {
            m_serviceSwitches[unitName]->setState(state == "active");
            qDebug() << "Updated state for" << unitName << "to" << state;
        }
    }
}

// Setup function
void setupSystemDWatcher()
{
    new SystemDWatcher(qApp);
}

REGISTER_INTEGRATION("SystemD", setupSystemDWatcher, true)

#include "systemd.moc"
