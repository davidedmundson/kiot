// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QAction>
#include <QDebug>
#include <QCoreApplication>
#include <KProcess>

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
            KProcess *p = new KProcess();
            p->setShellCommand(exec);
            p->startDetached();
            delete p;
        });
    }
}
REGISTER_INTEGRATION("Scripts",registerScripts,true)
