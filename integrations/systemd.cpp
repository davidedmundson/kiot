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
    void onPropertiesChanged(const QString &interface, const QVariantMap &changedProps, const QStringList &invalidatedProps, const QDBusMessage &msg);

private:
    KSharedConfig::Ptr cfg;
    QHash<QString, Switch*> m_serviceSwitches;
    QDBusInterface *m_systemdUser = nullptr;
    QString sanitizeServiceId(const QString &svc);
    QStringList listUserServices() const;
};

SystemDWatcher::SystemDWatcher(QObject *parent)
    : QObject(parent)
{
    cfg = KSharedConfig::openConfig("kiotrc");
    if (!ensureConfig()) {
        qWarning() << "SystemD: Failed to ensure config";
        return;
    }
    if (!init()) {
        qWarning() << "SystemD: Initialization failed due to config errors, aborting";
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

    // Listen for PropertiesChanged signals from systemd units
    QDBusConnection::sessionBus().connect(
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage))
    );
}

// Create config entries for all available user services
// TODO: Remove services from config that are no longer available, while keeping state of existing services
bool SystemDWatcher::ensureConfig()
{
    KConfigGroup intgrp(cfg, "Integrations");
    if (!intgrp.readEntry("SystemD", false)) {
        qWarning() << "Aborting: SystemD integration disabled, should not be running";
        return false;
    }

    KConfigGroup grp(cfg, "systemd");
    if (!grp.exists()) {
        for (const QString &svc : listUserServices()) {
            grp.writeEntry(svc, false); // default: disabled
        }
        cfg->sync();
    }
    return true;
}

bool SystemDWatcher::init()
{
    KConfigGroup grp(cfg, "systemd");
    if (!grp.exists()) 
        return false; // Early return if config is missing, should not happen though

    // Create switches for all enabled services
    auto services = listUserServices();
    for (const QString &svc : services) {
        if (!grp.hasKey(svc) || !grp.readEntry(svc, false))
            continue; // skip disabled services

        qDebug() << "SystemDWatcher: Adding service" << svc;
        auto *sw = new Switch(this);
        QString id = sanitizeServiceId(svc);
        sw->setId("systemd_" + id);
        sw->setName(svc);
        // TODO: Choose a suitable icon for systemd services
        sw->setState(false); // initial state; will be updated via D-Bus

        connect(sw, &Switch::stateChangeRequested, this, [this, svc](bool state) {
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
    id.replace(QRegularExpression("[^a-zA-Z0-9]"), '_');
    return id;
}

QStringList SystemDWatcher::listUserServices() const
{
    // Only list *.service under --user
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

void SystemDWatcher::onPropertiesChanged(const QString &interface, const QVariantMap &changedProps, const QStringList &invalidatedProps, const QDBusMessage &msg)
{
    Q_UNUSED(invalidatedProps)

    if (interface != "org.freedesktop.systemd1.Unit")
        return;

    QString path = msg.path();
    QString name = path.section('/', -1);

    if (!m_serviceSwitches.contains(name))
        return;

    if (changedProps.contains("ActiveState")) {
        QString state = changedProps["ActiveState"].toString();
        m_serviceSwitches[name]->setState(state == "active");
    }
}

void setupSystemDWatcher()
{
    new SystemDWatcher(qApp);
}

REGISTER_INTEGRATION("SystemD", setupSystemDWatcher, true)

#include "systemd.moc"
