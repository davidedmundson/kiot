#include "core.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QAction>
#include <QDebug>
#include <QCoreApplication>
#include <KGlobalAccel>
#include <QProcess>

void registerScripts()
{
    qInfo() << "Loading scripts";
    auto scriptConfigToplevel = KSharedConfig::openConfig()->group("Scripts");
    const QStringList scriptIds = scriptConfigToplevel.groupList();
    for (const QString &scriptId : scriptIds) {
        auto scriptConfig = scriptConfigToplevel.group(scriptId);
        const QString name = scriptConfig.readEntry("Name", scriptId);
        const QString exec = scriptConfig.readEntry("Exec");

        if (exec.isEmpty()) {
            qWarning() << "Could not find script Exec entry for" << scriptId;
            continue;
        }

        auto button = new Button(qApp);
        button->setId(scriptId);
        button->setName(name);
        // Home assistant integration supports payloads, which we could expose as args
        // maybe via some substitution in the exec line
        QObject::connect(button, &Button::triggered, qApp, [exec, scriptId]() {
            qInfo() << "Running script " << scriptId;
            // DAVE TODO flatpak escaping
            QProcess::startDetached(exec);
        });
    }
}
REGISTER_INTEGRATION(registerScripts)
