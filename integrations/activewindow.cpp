#include "core.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QTemporaryFile>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

class ActiveWindowWatcher : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.davidedmundson.kiot.ActiveWindow")
    
public:
    explicit ActiveWindowWatcher(QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE void UpdateTitle(const QString &title);

private slots:
    void onActiveWindowChanged(const QString &title);


private:
    bool registerKWinScript();
    void publish(const QString &state);

    Sensor *m_sensor;
    QDBusInterface *m_kwinIface = nullptr;
    QString m_lastTitle;
    QString m_scriptPath;
    bool m_connected = false;
};

ActiveWindowWatcher::ActiveWindowWatcher(QObject *parent)
    : QObject(parent)
{
    m_sensor = new Sensor(this);
    m_sensor->setId("active_window");
    m_sensor->setName("Active Window");
    m_sensor->setDiscoveryConfig("icon", "mdi:application");
    m_sensor->setState("Loading...");

    // Register DBus service first
    if (!QDBusConnection::sessionBus().registerService("org.davidedmundson.kiot.ActiveWindow")) {
        qWarning() << "ActiveWindowWatcher: Failed to register DBus service";
        m_sensor->setState("Unavailable");
        return;
    }
    
    // Register the object with the proper interface
    if (!QDBusConnection::sessionBus().registerObject("/ActiveWindow", "org.davidedmundson.kiot.ActiveWindow", this, QDBusConnection::ExportAllSlots)) {
        qWarning() << "ActiveWindowWatcher: Failed to register DBus object";
        m_sensor->setState("Unavailable");
        return;
    }

    if (!registerKWinScript()) {
        qWarning() << "ActiveWindowWatcher: Failed to register KWin script";
        m_sensor->setState("Unavailable");
        return;
    }

}
QString detectPlasmaVersion()
{
    QByteArray val = qgetenv("KDE_SESSION_VERSION");
    if (val.isEmpty())
        return QString(); // unknown

    return QString::fromUtf8(val);
}
bool ActiveWindowWatcher::registerKWinScript()
{
    m_kwinIface = new QDBusInterface(
        "org.kde.KWin",
        "/Scripting",
        "org.kde.kwin.Scripting",
        QDBusConnection::sessionBus(),
        this
    );

    if (!m_kwinIface->isValid()) {
        qWarning() << "ActiveWindowWatcher: KWin scripting interface not available";
        return false;
    }

    // Clean up any existing instances
    QDBusMessage isLoadedReply = m_kwinIface->call("isScriptLoaded", "kiot_activewindow");
    if (isLoadedReply.type() != QDBusMessage::ErrorMessage && !isLoadedReply.arguments().isEmpty()) {
        bool isLoaded = isLoadedReply.arguments().first().toBool();
        if (isLoaded) {
            m_kwinIface->call("unloadScript", "kiot_activewindow");
        }
    }

    // Create script in runtime directory
    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) {
        runtimeDir = "/tmp";
    }
    
    QString scriptDir = runtimeDir + "/kiot";
    QDir().mkpath(scriptDir);
    
    m_scriptPath = scriptDir + "/kwin_activewindow.js";
    
    QFile scriptFile(m_scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "ActiveWindowWatcher: could not open script file" << m_scriptPath;
        return false;
    }

    QTextStream out(&scriptFile);
    QString plasmaVersion = detectPlasmaVersion();
    if (plasmaVersion == "5") {
        out << "var lastCaption = '';\n"
            "workspace.clientActivated.connect(function(client) {\n"
            "    if (!client) {\n"
            "        if (lastCaption !== '') {\n"
            "            lastCaption = '';\n"
            "            callDBus('org.davidedmundson.kiot.ActiveWindow', '/ActiveWindow', 'org.davidedmundson.kiot.ActiveWindow', 'UpdateTitle', '');\n"
            "        }\n"
            "        return;\n"
            "    }\n"
            "    var caption = client.caption || client.resourceClass || '';\n"
            "    if (caption !== lastCaption) {\n"
            "        lastCaption = caption;\n"
            "        callDBus('org.davidedmundson.kiot.ActiveWindow', '/ActiveWindow', 'org.davidedmundson.kiot.ActiveWindow', 'UpdateTitle', caption);\n"
            "    }\n"
            "});\n";
    }
    else {
        out << "var lastCaption = '';\n"
            "workspace.windowActivated.connect(window => {\n"
            "    if (!window) {\n"
            "        if (lastCaption !== '') {\n"
            "            lastCaption = '';\n"
            "            callDBus('org.davidedmundson.kiot.ActiveWindow', '/ActiveWindow', 'org.davidedmundson.kiot.ActiveWindow', 'UpdateTitle', '');\n"
            "        }\n"
            "        return;\n"
            "    }\n"
            "    var caption = window.cation ?? window.resourceClass ?? '';;\n"
            "    if (caption !== lastCaption) {\n"
            "        lastCaption = caption;\n"
            "        callDBus('org.davidedmundson.kiot.ActiveWindow', '/ActiveWindow', 'org.davidedmundson.kiot.ActiveWindow', 'UpdateTitle', caption);\n"
            "    }\n"
            "});\n";}

    scriptFile.flush();
    scriptFile.close();

    // Set proper permissions
    QFile::setPermissions(m_scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

    QDBusMessage reply = m_kwinIface->call("loadScript", m_scriptPath, "kiot_activewindow");
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "ActiveWindowWatcher: loadScript failed:" << reply.errorMessage();
        QFile::remove(m_scriptPath);
        return false;
    }

    if (reply.arguments().isEmpty()) {
        qWarning() << "ActiveWindowWatcher: loadScript returned no arguments";
        QFile::remove(m_scriptPath);
        return false;
    }

    QVariant arg = reply.arguments().first();
    QString scriptObjectPath;

    if (arg.canConvert<QDBusObjectPath>()) {
        QDBusObjectPath path = arg.value<QDBusObjectPath>();
        scriptObjectPath = path.path();
        //qDebug() << "ActiveWindowWatcher: loadScript returned object path" << scriptObjectPath;
    } else if (arg.canConvert<int>()) {
        int id = arg.toInt();
        scriptObjectPath = QString("/Scripting/Script%1").arg(id);
        //qDebug() << "ActiveWindowWatcher: loadScript returned int id" << id << "-> path" << scriptObjectPath;
    } else if (arg.canConvert<QString>()) {
        scriptObjectPath = arg.toString();
        if (!scriptObjectPath.startsWith("/"))
            scriptObjectPath.prepend("/Scripting/");
        //qDebug() << "ActiveWindowWatcher: loadScript returned string" << scriptObjectPath;
    } else {
        qWarning() << "ActiveWindowWatcher: Unknown return type from loadScript" << arg.typeName();
        QFile::remove(m_scriptPath);
        return false;
    }

    QDBusInterface scriptIface(
        "org.kde.KWin",
        scriptObjectPath,
        "org.kde.kwin.Script",
        QDBusConnection::sessionBus()
    );

    if (!scriptIface.isValid()) {
        qWarning() << "ActiveWindowWatcher: scriptIface invalid for path" << scriptObjectPath;
        QFile::remove(m_scriptPath);
        return false;
    }

    QDBusMessage runReply = scriptIface.call("run");
    if (runReply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "ActiveWindowWatcher: run failed:" << runReply.errorMessage();
        QFile::remove(m_scriptPath);
        return false;
    }

    //qDebug() << "ActiveWindowWatcher: loaded script successfully at" << scriptObjectPath;
    return true;
}

Q_SCRIPTABLE void ActiveWindowWatcher::UpdateTitle(const QString &title)
{
    //qDebug() << "ActiveWindowWatcher: UpdateTitle called with:" << title;
    onActiveWindowChanged(title);
}

void ActiveWindowWatcher::onActiveWindowChanged(const QString &title)
{
    if (title != m_lastTitle) {
        m_lastTitle = title;
        publish(title);
    }
}

void ActiveWindowWatcher::publish(const QString &state)
{
    if (!state.isEmpty()){
    m_sensor->setState(state.isEmpty() ? "No active window" : state);
    //qDebug() << "ActiveWindowWatcher: updated title:" << state;
    }
}

void setupActiveWindow()
{
    new ActiveWindowWatcher(qApp);
}

REGISTER_INTEGRATION(setupActiveWindow)
#include "activewindow.moc"


