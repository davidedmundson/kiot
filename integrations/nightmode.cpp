#include "core.h"
#include "dbusproperty.h"
#include <QCoreApplication>

#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusReply>

class NightMode : public QObject
{
    Q_OBJECT
public:
    NightMode(QObject *parent);
private:
    BinarySensor *m_sensor;
    Switch *m_switch;
    std::optional<uint32_t> m_inhibitCookie;
};

NightMode::NightMode(QObject *parent)
    : QObject(parent)
{
    m_sensor = new BinarySensor(this);
    m_sensor->setId("nightmode_inhibited");
    m_sensor->setName("Night Mode Inhibited");

    auto nightmodeInhibited = new DBusProperty("org.kde.KWin", "/org/kde/KWin/NightLight", "org.kde.KWin.NightLight", "inhibited", this);
    QObject::connect(nightmodeInhibited, &DBusProperty::valueChanged, this, [this](const QVariant &value) {
        m_sensor->setState(value.toBool());
    });
    m_sensor->setState(nightmodeInhibited->value().toBool());

    m_switch = new Switch(this);
    m_switch->setId("nightmode_inhibit");
    m_switch->setName("Night ode Inihibt");
    m_switch->setState(false); // the state is whether this switch is inhibiting the night mode, the sensor is if /anything/ is
    QObject::connect(m_switch, &Switch::stateChangeRequested, this, [this](bool state) {
        if (state) {
            QDBusMessage inhibitCall = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin.NightLight"),
                                                                      QStringLiteral("/org/kde/KWin/NightLight"),
                                                                      QStringLiteral("org.kde.KWin.NightLight"),
                                                                      QStringLiteral("inhibit"));
            QDBusReply<uint32_t> reply = QDBusConnection::sessionBus().call(inhibitCall);
            if (!reply.isValid()) {
                qWarning() << "Failed to inhibit nightmode";
            }
            m_inhibitCookie = reply.value();
        } else if (m_inhibitCookie.has_value()){
            QDBusMessage uninhibitCall = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin.NightLight"),
                                                                      QStringLiteral("/org/kde/KWin/NightLight"),
                                                                      QStringLiteral("org.kde.KWin.NightLight"),
                                                                      QStringLiteral("uninhibit"));
            uninhibitCall << m_inhibitCookie.value();
            QDBusConnection::sessionBus().call(uninhibitCall);
        }
        m_switch->setState(state);
    });
}

void setupNightmode()
{
    new NightMode(qApp);
}

REGISTER_INTEGRATION(setupNightmode)

#include "nightmode.moc"
