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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

class ActiveWindowWatcher : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.davidedmundson.kiot.ActiveWindow")

public:
    explicit ActiveWindowWatcher(QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE void UpdateAttributes(const QString &json);


private:
    bool registerKWinScript();

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
    // Register DBus service first
    if (!QDBusConnection::sessionBus().registerService("org.davidedmundson.kiot.ActiveWindow")) {
        qWarning() << "ActiveWindowWatcher: Failed to register DBus service";
        m_sensor->setState("Unavailable");
        return;
    }

    if (!QDBusConnection::sessionBus().registerObject("/ActiveWindow",
            "org.davidedmundson.kiot.ActiveWindow",
            this,
            QDBusConnection::ExportAllSlots)) {
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

    // Clean up any existing instance
    QDBusMessage isLoadedReply = m_kwinIface->call("isScriptLoaded", "kiot_activewindow");
    if (isLoadedReply.type() != QDBusMessage::ErrorMessage && !isLoadedReply.arguments().isEmpty()) {
        bool isLoaded = isLoadedReply.arguments().first().toBool();
        if (isLoaded)
            m_kwinIface->call("unloadScript", "kiot_activewindow");
    }

    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty())
        runtimeDir = "/tmp";
    QString scriptDir = runtimeDir + "/kiot";
    QDir().mkpath(scriptDir);
    m_scriptPath = scriptDir + "/kwin_activewindow.js";

    QFile scriptFile(m_scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "ActiveWindowWatcher: could not open script file" << m_scriptPath;
        return false;
    }

    QTextStream out(&scriptFile);
    out << "var lastPayload = {};\n"
        "var currentWindow = null;\n"
        "\n"
        "function updateActiveWindow(w) {\n"
        "    if (!w) return;\n"
        "    if (w.transient && w.transientFor) w = w.transientFor;"
        "\n"
        "    var payload = {\n"
        "        title: w.caption || '',\n"
        "        resourceClass: w.resourceClass || '',\n"
        "        fullscreen: w.fullScreen.toString(),\n"
        "        screen: w.output.manufacturer,\n"
        "        x: w.x,\n"
        "        y: w.y,\n"
        "        width: w.width,\n"
        "        height: w.height,\n"
        "        pid: w.pid\n"
        "    };\n"
        "\n"
        "    var payloadStr = JSON.stringify(payload);\n"
        "    if (payloadStr !== JSON.stringify(lastPayload)) {\n"
        "        lastPayload = payload;\n"
        "        callDBus('org.davidedmundson.kiot.ActiveWindow', '/ActiveWindow', 'org.davidedmundson.kiot.ActiveWindow', 'UpdateAttributes', payloadStr);\n"
        "    }\n"
        "}\n"
        "\n"
        "function onCaptionChanged() {\n"
        "    updateActiveWindow(currentWindow);\n"
        "}\n"
        "\n"
        "function onGeometryChanged() {\n"
        "    updateActiveWindow(currentWindow);\n"
        "}\n"
        "\n"
        "function watchWindow(w) {\n"
        "    if (!w) return;\n"
        "\n"
        "    if (currentWindow) {\n"
        "        currentWindow.captionChanged.disconnect(onCaptionChanged);\n"
        "        currentWindow.frameGeometryChanged.disconnect(onGeometryChanged);\n"
        "    }\n"
        "\n"
        "    currentWindow = w;\n"
        "    currentWindow.captionChanged.connect(onCaptionChanged);\n"
        "    currentWindow.frameGeometryChanged.connect(onGeometryChanged);\n"
        "\n"
        "    updateActiveWindow(w);\n"
        "}\n"
        "\n"
        "watchWindow(workspace.activeWindow);\n"
        "workspace.windowActivated.connect(watchWindow);\n";
        

    scriptFile.flush();
    scriptFile.close();
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

    // KWin returns an int for the script ID in all supported versions
    if (arg.canConvert<int>()) {
        int id = arg.toInt();
        scriptObjectPath = QString("/Scripting/Script%1").arg(id);
    } else {
        qWarning() << "ActiveWindowWatcher: Unexpected return type from loadScript:" << arg.typeName();
        QFile::remove(m_scriptPath);
        return false;
    }
    QDBusInterface scriptIface("org.kde.KWin", scriptObjectPath, "org.kde.kwin.Script", QDBusConnection::sessionBus());
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

    return true;
}


void ActiveWindowWatcher::UpdateAttributes(const QString &json)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;

    QVariantMap attrs = doc.object().toVariantMap();
    QString title = attrs["title"].toString();
    if (title != m_lastTitle) {
        m_lastTitle = title;
        m_sensor->setState(title);
    }
    m_sensor->setAttributes(attrs);
    
}




void setupActiveWindow()
{
    new ActiveWindowWatcher(qApp);
}

REGISTER_INTEGRATION(setupActiveWindow)
#include "activewindow.moc"
