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

        qCDebug(shortcut) << "Executing shortcut:" << newOption;
        
        // Execute shortcut via DBus
        QDBusInterface kglobalaccel("org.kde.kglobalaccel", 
                                     "/component/kwin", 
                                     "org.kde.kglobalaccel.Component",
                                     QDBusConnection::sessionBus());
        
        if (kglobalaccel.isValid()) {
            QDBusReply<void> reply = kglobalaccel.call("invokeShortcut", newOption);
            if (!reply.isValid()) {
                qCWarning(shortcut) << "Failed to execute shortcut" << newOption << ":" << reply.error().message();
                
                // Try other common components
                QStringList components = {"kwin", "krunner", "plasmashell", "org.kde.kglobalaccel"};
                for (const QString &component : components) {
                    QDBusInterface altInterface("org.kde.kglobalaccel", 
                                                QString("/component/%1").arg(component), 
                                                "org.kde.kglobalaccel.Component",
                                                QDBusConnection::sessionBus());
                    if (altInterface.isValid()) {
                        QDBusReply<void> altReply = altInterface.call("invokeShortcut", newOption);
                        if (altReply.isValid()) {
                            qCDebug(shortcut) << "Executed shortcut" << newOption << "via component" << component;
                            break;
                        }
                    }
                }
            } else {
                qCDebug(shortcut) << "Successfully executed shortcut:" << newOption;
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
    // Expose shortcuts as a select entity
    void exposeShortcuts()
    {
        m_shortcutSelect = new Select(this);
        m_shortcutSelect->setId("shortcuts");
        m_shortcutSelect->setName("Shortcuts");
        
        QStringList shortcutIds;
        shortcutIds.append("Default");
        
        // Get all shortcuts from DBus
        QDBusInterface kglobalaccel("org.kde.kglobalaccel", 
                                     "/component/kwin", 
                                     "org.kde.kglobalaccel.Component",
                                     QDBusConnection::sessionBus());
        
        if (kglobalaccel.isValid()) {
            QDBusReply<QStringList> reply = kglobalaccel.call("shortcutNames");
            if (reply.isValid()) {
                QStringList systemShortcuts = reply.value();
                shortcutIds.append(systemShortcuts);
                qCDebug(shortcut) << "Found" << systemShortcuts.size() << "system shortcuts";
            } else {
                qCWarning(shortcut) << "Failed to get shortcut names:" << reply.error().message();
                
                // Try to get shortcuts from all available components
                QStringList components = getGlobalAccelComponents();
                for (const QString &component : components) {
                    QDBusInterface componentInterface("org.kde.kglobalaccel", 
                                                      QString("/component/%1").arg(component), 
                                                      "org.kde.kglobalaccel.Component",
                                                      QDBusConnection::sessionBus());
                    if (componentInterface.isValid()) {
                        QDBusReply<QStringList> componentReply = componentInterface.call("shortcutNames");
                        if (componentReply.isValid()) {
                            QStringList componentShortcuts = componentReply.value();
                            for (const QString &shortcut : componentShortcuts) {
                                shortcutIds.append(QString("%1/%2").arg(component).arg(shortcut));
                            }
                            qCDebug(shortcut) << "Found" << componentShortcuts.size() << "shortcuts in component" << component;
                        }
                    }
                }
            }
        } else {
            qCWarning(shortcut) << "Could not connect to org.kde.kglobalaccel";
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
                                     "/", 
                                     "org.kde.kglobalaccel",
                                     QDBusConnection::sessionBus());
        
        if (kglobalaccel.isValid()) {
            QDBusReply<QStringList> reply = kglobalaccel.call("components");
            if (reply.isValid()) {
                components = reply.value();
            }
        }
        
        // Fallback to common components
        if (components.isEmpty()) {
            components = {"kwin", "krunner", "plasmashell", "org.kde.kglobalaccel"};
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
};

void setupShortcuts()
{
    new Shortcut(qApp);
}

REGISTER_INTEGRATION("Shortcuts", setupShortcuts, true)

#include "shortcuts.moc"