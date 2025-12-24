// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/switch.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QRegularExpression>

#include <KConfigGroup>
#include <KProcess>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QTimer>
Q_DECLARE_LOGGING_CATEGORY(SystemD)
Q_LOGGING_CATEGORY(SystemD, "integration.SystemD")

class SystemDWatcher : public QObject
{
    Q_OBJECT
public:
    explicit SystemDWatcher(QObject *parent = nullptr);
    ~SystemDWatcher() = default;

    bool ensureConfig();

private slots:
    void onUnitPropertiesChanged(const QString &interface, const QVariantMap &changedProps, const QStringList &invalidatedProps, const QDBusMessage &msg);
    void performInit();

private:
    KSharedConfig::Ptr cfg;
    QHash<QString, Switch *> m_serviceSwitches;
    QDBusInterface *m_systemdUser = nullptr;
    QString sanitizeServiceId(const QString &svc);
    QStringList listUserServices() const;

    QString pathToUnitName(const QString &path) const;
    bool m_initialized = false;
};

namespace
{
static const QRegularExpression invalidCharRegex("[^a-zA-Z0-9]");
}

SystemDWatcher::SystemDWatcher(QObject *parent)
    : QObject(parent)
{
    cfg = KSharedConfig::openConfig();

    m_systemdUser =
        new QDBusInterface("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", QDBusConnection::sessionBus(), this);

    if (!m_systemdUser->isValid()) {
        qCWarning(SystemD) << "Failed to connect to systemd user D-Bus";
        return;
    }
    if (!ensureConfig()) {
        qCWarning(SystemD) << "Failed to ensure config";
        return;
    }
    performInit();
}

// keeps the systemd config group in sync with the actual services available
bool SystemDWatcher::ensureConfig()
{
    KConfigGroup grp(cfg, "systemd");

    const QStringList currentServices = listUserServices();
    if (currentServices.isEmpty()) {
        qCDebug(SystemD) << "No systemd services found";
        return false;
    }

    bool configChanged = false;
    for (const QString &svc : currentServices) {
        if (!grp.hasKey(svc)) {
            grp.writeEntry(svc, false);
            configChanged = true;
            qCDebug(SystemD) << "Added new service to config:" << svc;
        }
    }
    const QStringList configServices = grp.keyList();
    // Remove services no longer available
    for (const QString &cfgSvc : configServices) {
        if (!currentServices.contains(cfgSvc) && cfgSvc != QLatin1String("initialized")) {
            grp.deleteEntry(cfgSvc);
            configChanged = true;
            qCDebug(SystemD) << "Removed unavailable service from config:" << cfgSvc;
        }
    }

    if (configChanged) {
        cfg->sync();
        qCDebug(SystemD) << "SystemD configuration synchronized";
    }

    return true;
}

void SystemDWatcher::performInit()
{
    if (m_initialized) {
        return;
    }
    KConfigGroup grp(cfg, "systemd");
    // Initialize switches for enabled services
    for (const QString &svc : listUserServices()) {
        if (!grp.hasKey(svc) || !grp.readEntry(svc, false))
            continue; // skip disabled

        auto *sw = new Switch(this);
        sw->setId("systemd_" + sanitizeServiceId(svc));
        sw->setName(sanitizeServiceId(svc));
        sw->setState(false); // temp
        // Query initial state from D-Bus
        QDBusReply<QDBusObjectPath> unitPathReply = m_systemdUser->call("LoadUnit", svc);
        if (unitPathReply.isValid()) {
            qCDebug(SystemD) << "Getting inital state for " << svc;
            QDBusObjectPath unitPath = unitPathReply.value();

            QDBusInterface unitIface("org.freedesktop.systemd1", unitPath.path(), "org.freedesktop.DBus.Properties", QDBusConnection::sessionBus());

            QDBusReply<QVariant> stateReply = unitIface.call("Get", "org.freedesktop.systemd1.Unit", "ActiveState");
            if (stateReply.isValid()) {
                sw->setState(stateReply.value().toString() == "active");
            } else {
                qCDebug(SystemD) << "Failed to get state for " << svc << ": " << stateReply.error().message();
            }

            // Listen for live property changes
            QDBusConnection::sessionBus().connect("org.freedesktop.systemd1",
                                                  unitPath.path(),
                                                  "org.freedesktop.DBus.Properties",
                                                  "PropertiesChanged",
                                                  this,
                                                  SLOT(onUnitPropertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));
        } else {
            qCWarning(SystemD) << "Failed to get unit path for " << svc << ": " << unitPathReply.error().message();
        }

        // Connect switch to D-Bus for toggling service (works in flatpak)
        connect(sw, &Switch::stateChangeRequested, this, [this, svc](bool state) {
            if (!m_systemdUser || !m_systemdUser->isValid()) {
                qCWarning(SystemD) << "SystemD: D-Bus interface not available for toggling service";
                return;
            }

            QString method = state ? "StartUnit" : "StopUnit";
            QString mode = "replace"; // replace existing job if any

            QDBusReply<QDBusObjectPath> reply = m_systemdUser->call(method, svc, mode);
            if (!reply.isValid()) {
                qCWarning(SystemD) << "SystemD: Failed to" << (state ? "start" : "stop") << "service" << svc << ":" << reply.error().message();
            } else {
                // qCDebug(SystemD) << "Toggled service" << svc << "to" << (state ? "start" : "stop");
            }
        });
        m_serviceSwitches[svc] = sw;
    }

    m_initialized = true;
    qCInfo(SystemD) << "SystemD: Initialized" << m_serviceSwitches.size() << "service switches";
}

QString SystemDWatcher::sanitizeServiceId(const QString &svc)
{
    QString id = svc;
    id.replace(invalidCharRegex, QStringLiteral("_"));
    return id;
}

// List all user services (*.service) - Use ListUnitFiles which actually works
QStringList SystemDWatcher::listUserServices() const
{
    QStringList services;

    QDBusMessage reply = m_systemdUser->call("ListUnitFiles");
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(SystemD) << "SystemD: ListUnitFiles failed:" << reply.errorMessage();
        return services;
    }

    const QDBusArgument arg = reply.arguments().first().value<QDBusArgument>();

    arg.beginArray();
    while (!arg.atEnd()) {
        arg.beginStructure();

        QString path;
        QString state;
        arg >> path >> state;

        arg.endStructure();

        const QString unit = QFileInfo(path).fileName();
        if (unit.endsWith(".service")) {
            services.append(unit);
        }
    }
    arg.endArray();

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
            qCInfo(SystemD) << "Updated state for" << unitName << "to" << (state == "active");
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