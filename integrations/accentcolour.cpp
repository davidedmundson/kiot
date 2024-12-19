#include "core.h"
#include <QCoreApplication>

#include <KSharedConfig>
#include <KConfigGroup>
#include <KConfigWatcher>


class AccentColourWatcher: public QObject {
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

REGISTER_INTEGRATION(setupAccentColour)

#include "accentcolour.moc"
