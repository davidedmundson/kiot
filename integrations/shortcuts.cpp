#include "core.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QAction>
#include <QDebug>
#include <QCoreApplication>
#include <KGlobalAccel>

void registerShortcuts()
{
    auto shortcutConfigToplevel = KSharedConfig::openConfig()->group("Shortcuts");
    const QStringList shortcutIds = shortcutConfigToplevel.groupList();
    for (const QString &shortcutId : shortcutIds) {
        auto shortcutConfig = shortcutConfigToplevel.group(shortcutId);
        const QString name = shortcutConfig.readEntry("Name", shortcutId);
        QAction *action = new QAction(name, qApp);
        action->setObjectName(shortcutId);

        auto event = new Event(qApp);
        event->setId(shortcutId);
        event->setName(name);

        KGlobalAccel::self()->setShortcut(action, {});
        QObject::connect(action, &QAction::triggered, event, &Event::trigger);

    }
}

REGISTER_INTEGRATION("Shortcuts",registerShortcuts,true)
