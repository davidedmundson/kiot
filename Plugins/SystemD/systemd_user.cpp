#include "systemd_user.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTimer>
#include <KSharedConfig>
#include <KConfigGroup>
DEFINE_PLUGIN_LOGGER(SystemD, SystemD)

namespace
{
static const QRegularExpression invalidCharRegex("[^a-zA-Z0-9]");
}

SystemDWatcher::SystemDWatcher(QObject *parent)
    : QObject(parent)
{
    m_systemdUser =
        new QDBusInterface("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", QDBusConnection::sessionBus(), this);

    if (!m_systemdUser->isValid()) {
        qCWarning(SystemD) << "Failed to connect to systemd user D-Bus";
        return;
    }
    // systemd sends the units' PropertiesChanged signals only while some client is subscribed;
    // the subscription ends with our bus connection
    m_systemdUser->asyncCall("Subscribe");
    if (!ensureConfig()) {
        qCWarning(SystemD) << "Failed to ensure config";
        return;
    }
    performInit();
}

// keeps the systemd config group in sync with the actual services available
bool SystemDWatcher::ensureConfig()
{
    auto config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    KConfigGroup settings(config, "systemd");

    const QStringList currentServices = listUserServices();
    if (currentServices.isEmpty()) {
        qCDebug(SystemD) << "No systemd services found";
        return false;
    }

    bool configChanged = false;
    for (const QString &svc : currentServices) {
        if (!settings.hasKey(svc)) {
            settings.writeEntry(svc, false);
            configChanged = true;
            qCDebug(SystemD) << "Added new service to config:" << svc;
        }
    }
    
    const QStringList configServices = settings.keyList(); 
    
    for (const QString &cfgSvc : configServices) {
        if (!currentServices.contains(cfgSvc) && cfgSvc != QLatin1String("initialized")) {
            settings.deleteEntry(cfgSvc);
            configChanged = true;
            qCDebug(SystemD) << "Removed unavailable service from config:" << cfgSvc;
        }
    }

    if (configChanged) {
        config->sync(); 
        qCDebug(SystemD) << "SystemD configuration synchronized";
    }
    return true;
}

void SystemDWatcher::performInit()
{
    if (m_initialized) {
        return;
    }
    
    auto config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    KConfigGroup settings(config, "systemd");
  
    for (const QString &svc : listUserServices()) {
        if (!settings.hasKey(svc) || !settings.readEntry(svc, false))
            continue; 

        auto *sw = new Switch(this);
        sw->setId("systemd_" + sanitizeServiceId(svc));
        sw->setName(sanitizeServiceId(svc));
        sw->setDiscoveryConfig("icon", "mdi:account-wrench");
        sw->setState(false); // temp
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

            QDBusConnection::sessionBus().connect("org.freedesktop.systemd1",unitPath.path(),"org.freedesktop.DBus.Properties","PropertiesChanged",this,SLOT(onUnitPropertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));
        } 
        else {
            qCWarning(SystemD) << "Failed to get unit path for " << svc << ": " << unitPathReply.error().message();
        }

        connect(sw, &Switch::stateChangeRequested, this, [this, svc](bool state) {
            if (!m_systemdUser || !m_systemdUser->isValid()) {
                qCWarning(SystemD) << "SystemD: D-Bus interface not available for toggling service";
                return;
            }

            QString method = state ? "StartUnit" : "StopUnit";
            QString mode = "replace"; 

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

// Convert D-Bus path to proper unit name. systemd escapes every byte outside [A-Za-z0-9] (and a
// leading digit) of the UTF-8 name as _XX hex; an invalid escape is taken literally, as systemd does.
QString SystemDWatcher::pathToUnitName(const QString &path)
{
    const QByteArray label = path.section('/', -1).toLatin1();
    if (label == "_") {
        return QString();
    }
    QByteArray name;
    for (int i = 0; i < label.size(); ++i) {
        if (label[i] == '_' && i + 2 < label.size()) {
            bool ok = false;
            const int byte = label.mid(i + 1, 2).toInt(&ok, 16);
            if (ok) {
                name.append(char(byte));
                i += 2;
                continue;
            }
        }
        name.append(label[i]);
    }
    return QString::fromUtf8(name);
}

// Slot for handling live updates from systemd units
void SystemDWatcher::onUnitPropertiesChanged(const QString &interface,const QVariantMap &changedProps,
                                           const QStringList &invalidatedProps,const QDBusMessage &msg)
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
