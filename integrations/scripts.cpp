// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/button.h"
#include <KConfigGroup>
#include <KProcess>
#include <KSharedConfig>
#include <KSandbox>

#include <QAction>
#include <QCoreApplication>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(scripts)
Q_LOGGING_CATEGORY(scripts, "integration.Scripts")

void registerScripts()
{
    auto scriptConfigToplevel = KSharedConfig::openConfig()->group("Scripts");
    const QStringList scriptIds = scriptConfigToplevel.groupList();
    for (const QString &scriptId : scriptIds) {
        auto scriptConfig = scriptConfigToplevel.group(scriptId);
        const QString name = scriptConfig.readEntry("Name", scriptId);
        const QString exec = scriptConfig.readEntry("Exec");
        const QString icon = scriptConfig.readEntry("icon","mdi:script-text");
        if (exec.isEmpty()) {
            qCWarning(scripts) << "Could not find script Exec entry for" << scriptId;
            continue;
        }

        auto button = new Button(qApp);
        button->setId(scriptId);
        button->setName(name);
        button->setDiscoveryConfig("icon", icon);
        // Home assistant integration supports payloads, which we could expose as args
        // maybe via some substitution in the exec line
        QObject::connect(button, &Button::triggered, qApp, [exec, scriptId]() {
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

            if (KSandbox::isFlatpak()) {
                KSandbox::ProcessContext ctx = KSandbox::makeHostContext(*p);
                p->setProgram(ctx.program);
                p->setArguments(ctx.arguments);
            }

            p->startDetached();
            delete p;
        });
    }
    if( scriptIds.length() >= 1 )
        qCInfo(scripts) << "Loaded" << scriptIds.length() << " scripts:" << scriptIds.join(", ");
}
REGISTER_INTEGRATION("Scripts", registerScripts, true)
