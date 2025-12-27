// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/event.h"
#include "entities/select.h"
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KSharedConfig>
#include <QAction>
#include <QTimer>
#include <QApplication>
#include <QStringList>
#include <QLoggingCategory>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>

Q_DECLARE_LOGGING_CATEGORY(shortcut)
Q_LOGGING_CATEGORY(shortcut, "integration.Shortcut")

class Shortcut : public QObject
{
    Q_OBJECT

public:
    explicit Shortcut(QObject *parent = nullptr)
        : QObject(parent)
        , m_shortcutSelect(nullptr)
    {
        registerShortcuts();
        exposeShortcuts();
    }

private slots:
    void onOptionSelected(const QString &newOption)
    {
        if (newOption == "Default" || !m_shortcutSelect) {
            return;
        }
        if(!m_shortcuts.contains(newOption))
            return;
        auto sc = m_shortcuts[newOption];
        qCDebug(shortcut) << "Executing shortcut" << sc.shortcutName << "from component" << sc.componentName;
        // Execute shortcut via DBus
        QDBusInterface kglobalaccel("org.kde.kglobalaccel", 
                                     QString("/component/" + sc.componentName), 
                                     "org.kde.kglobalaccel.Component",
                                     QDBusConnection::sessionBus());
        
        if (kglobalaccel.isValid()) {
            QDBusReply<void> reply = kglobalaccel.call("invokeShortcut", sc.shortcutName);
            if (reply.isValid()) {
                qCDebug(shortcut) << "Successfully executed shortcut:" << sc.shortcutName << "from component" << sc.componentName;
            } else {
                qCDebug(shortcut) << "Failed to execute shortcut:" << sc.shortcutName << "from component" << sc.componentName;
            }
        } else {
            qCWarning(shortcut) << "Could not connect to org.kde.kglobalaccel";
        }

        // Return to default
        if (m_shortcutSelect) {
            QTimer::singleShot(100, this, [this]() {
                m_shortcutSelect->setState("Default");
            });
        }
    }

private:
    struct ShortcutDbus {
        QString componentName;
        QString shortcutName;
        QString keyCombo;
    };



    // Expose shortcuts as a select entity
    void exposeShortcuts()
    {
        m_shortcutSelect = new Select(this);
        m_shortcutSelect->setId("shortcuts");
        m_shortcutSelect->setName("Shortcuts");
        
        QStringList shortcutIds;
        shortcutIds.append("Default");
        

        // Try to get shortcuts from all available components
        QStringList components = getGlobalAccelComponents();
        for (const QString &component : components) {
            QDBusInterface componentInterface("org.kde.kglobalaccel", 
                                            QString("%1").arg(component), 
                                       "org.kde.kglobalaccel.Component",
                                      QDBusConnection::sessionBus());
            if (componentInterface.isValid()) {
                QDBusReply<QStringList> componentReply = componentInterface.call("shortcutNames");
                if (componentReply.isValid()) {
                    QStringList componentShortcuts = componentReply.value();
                    for (const QString &shortcut : componentShortcuts) {
                        ShortcutDbus dd;
                        dd.componentName = component;
                        dd.shortcutName = shortcut;
                        m_shortcuts[shortcut] = dd;
                        shortcutIds.append(QString("%1").arg(shortcut));
                    }
                    qCDebug(shortcut) << "Found" << componentShortcuts.size() << "shortcuts in component" << component;
                }
            }
        }
        
        m_shortcutSelect->setOptions(shortcutIds);
        m_shortcutSelect->setState("Default");
        
        connect(m_shortcutSelect, &Select::optionSelected, this, &Shortcut::onOptionSelected);
        
        qCInfo(shortcut) << "Exposed" << shortcutIds.size() << "shortcuts in select entity";
    }
    
    QStringList getGlobalAccelComponents() const
    {
        QStringList components;
        
        QDBusInterface kglobalaccel("org.kde.kglobalaccel", 
                                     "/kglobalaccel", 
                                     "org.kde.KGlobalAccel",
                                     QDBusConnection::sessionBus());
        
        if (kglobalaccel.isValid()) {
            QDBusReply<QList<QDBusObjectPath>> reply = kglobalaccel.call("allComponents");
            if (reply.isValid()) {
                for (const QDBusObjectPath& path : reply.value()) 
                {
                    if(path.path().isEmpty()) continue;
                    components.append(path.path());
                }
            }
        }
           
        // Fallback to common components
        if (components.isEmpty()) {
             components = {"kwin", "krunner", "plasmashell", "org.kde.kglobalaccel", "com_obsproject_Studio"};
        
        }
        
        return components;
    }

    // Register our shortcuts to allow events to be executed from it to trigger automations
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
        
        if (shortcutIds.length() >= 1) {
            qCInfo(shortcut) << "Registered" << shortcutIds.length() << "custom shortcuts:" << shortcutIds.join(", ");
        }
    }

    Select *m_shortcutSelect;
    QMap<QString, ShortcutDbus> m_shortcuts;
};

void setupShortcuts()
{
    new Shortcut(qApp);
}

REGISTER_INTEGRATION("Shortcuts", setupShortcuts, true)

#include "shortcuts.moc"