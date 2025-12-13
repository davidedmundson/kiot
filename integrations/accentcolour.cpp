// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/entities.h"
#include <QCoreApplication>

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>

class AccentColourWatcher : public QObject
{
    Q_OBJECT
public:
    AccentColourWatcher(QObject *parent = nullptr)
        : QObject(parent)
    {
        auto sensor = new Sensor(this);
        sensor->setId("accentcolor");
        sensor->setName("Accent Color");

        // it's in kdeglobals
        KConfigGroup config(KSharedConfig::openConfig()->group("General"));
        sensor->setState(config.readEntry("AccentColor")); // if not custom, then we should find out the default from the theme?

        m_watcher = KConfigWatcher::create(KSharedConfig::openConfig());

        QObject::connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [sensor](const KConfigGroup &group) {
            if (group.name() != "General") {
                return;
            }
            // this is in the format "r,g,b" as numbers. Will need some conversion HA side to do anything useful with it
            sensor->setState(group.readEntry("AccentColor"));
        });
    }

private:
    KConfigWatcher::Ptr m_watcher;
};

void setupAccentColour()
{
    new AccentColourWatcher(qApp);
}

REGISTER_INTEGRATION("AccentColour", setupAccentColour, true)

#include "accentcolour.moc"
