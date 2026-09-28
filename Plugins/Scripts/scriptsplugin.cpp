#include "scriptsplugin.h"
#include <QCoreApplication>

#include <KIOTShared/kiotshared.h>
#include "core/core.h"

using KIOTShared::Entities::Button;
using KIOTShared::PlatformHelper;

#include <KConfigGroup>
#include <KProcess>
#include <KSharedConfig>
#include <QCoreApplication>
#include <QStringList>

DEFINE_PLUGIN_LOGGER(scripts, Scripts)

DEFINE_PLUGIN_LOGGER(plugin_logger,PLUGIN_NAME)

ScriptsPlugin::ScriptsPlugin(QObject *parent)
    : QObject(parent)
{
}

QString ScriptsPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString ScriptsPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl ScriptsPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber ScriptsPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}


bool ScriptsPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasFlatpakTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.freedesktop.Flatpak", "talk");
        if (!hasFlatpakTalk) {
            qCWarning(customSensors) << "Missing required Flatpak permission to talk to org.freedesktop.Flatpak.";
            qCInfo(customSensors) << "To fix this, run:";
            qCInfo(customSensors) << "  flatpak --user override --talk-name=org.freedesktop.Flatpak " + appId;
            return false;
        }
    }
    return true;
}
bool ScriptsPlugin::enabledByDefault()
{
    if (!checkCompatibility()) {
        return false;
    }
    return true; 
}

bool ScriptsPlugin::startPlugin()
{
    if(m_container)
        stopPlugin();

    m_container = new QObject(this);

    auto scriptConfigToplevel = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig)->group("Scripts");
    const QStringList scriptIds = scriptConfigToplevel.groupList();
    
    for (const QString &scriptId : scriptIds) {
        auto scriptConfig = scriptConfigToplevel.group(scriptId);
        const QString name = scriptConfig.readEntry("Name", scriptId);
        const QString exec = scriptConfig.readEntry("Exec");
        const QString icon = scriptConfig.readEntry("icon", "mdi:script-text");
        
        if (exec.isEmpty()) {
            qCWarning(scripts) << "Could not find script Exec entry for" << scriptId;
            continue;
        }

        auto button = new Button(m_container);
        button->setId(scriptId);
        button->setName(name);
        button->setDiscoveryConfig("icon", icon);

        QObject::connect(button, &Button::triggered, m_container, [exec, scriptId]() {
            qCInfo(scripts) << "Running script " << scriptId;
            QStringList args = QProcess::splitCommand(exec); 
            if (args.isEmpty()) {                            
                qCWarning(scripts) << "Could not parse script Exec entry for" << scriptId;
                return;
            } 
            QString program = args.takeFirst();           

            KProcess *p = new KProcess();
            p->setProgram(program);
            p->setArguments(args);

            if (PlatformHelper::isFlatpak()) {
                PlatformHelper::ProcessContext ctx = PlatformHelper::makeHostContext(*p);
                p->setProgram(ctx.program);
                p->setArguments(ctx.arguments);
            }

            p->startDetached();
            delete p;
        });
    }
    
    if (!scriptIds.isEmpty()) {
        qCInfo(scripts) << "Loaded and started" << scriptIds.length() << "scripts:" << scriptIds.join(", ");
    }
    
    return true;
}

bool ScriptsPlugin::stopPlugin()
{
    if (m_container) {
        delete m_container;
        m_container = nullptr;
        qCInfo(scripts) << "Stopped Scripts plugin and cleaned up resources.";
    }
    return true;
}

#include "scriptsplugin.moc"