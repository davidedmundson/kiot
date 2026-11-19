// SPDX-FileCopyrightText: 2026 Thomas Kastenholz <2893148+Thokas@users.noreply.github.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

class QDBusServiceWatcher;

namespace KIOTShared::Entities
{
class Sensor;
}

/**
 * @brief Sensors for what currently keeps the session from suspending and the screen from dimming/locking.
 *
 * Reads PowerDevil's PolicyAgent, which collects the inhibitions of applications (org.freedesktop.PowerManagement,
 * org.freedesktop.ScreenSaver, ...) and the "block" inhibitors other processes hold in logind
 * (e.g. `systemd-inhibit --what=sleep`). Each inhibition says what it covers, logind style:
 * "sleep" = suspend, "idle" = the idle actions for the screen (dim, turn off, automatic lock).
 *
 * - "Suspend blockers" (suspend_blockers): the number of active inhibitions covering sleep.
 * - "Screen blockers" (screen_blockers): the number covering idle (incl. those covering sleep too).
 *
 * Attributes of both: `summary` ("who: why; ..." or "none"), `inhibitions` (who, why, what) and `source`.
 *
 * Plasma >= 6.5: the ActiveInhibitions property, updated on PropertiesChanged. Older PowerDevil: the deprecated
 * ListInhibitions method and InhibitionsChanged signal, which don't say what an inhibition covers, so there every
 * active inhibition counts for both. Without PowerDevil on the session bus both are unknown (payload "None").
 */
class PowerInhibitions : public QObject
{
    Q_OBJECT
public:
    explicit PowerInhibitions(QObject *parent = nullptr);
    ~PowerInhibitions() override;

    struct Inhibition {
        QString what; // logind style, e.g. "sleep:idle"; empty when PowerDevil doesn't say (legacy API)
        QString who;
        QString why;
        bool operator==(const Inhibition &other) const = default;
    };

private Q_SLOTS:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);
    void onLegacyInhibitionsChanged();

private:
    void refresh();
    void refreshLegacy();
    void setUnknown();
    void publish(const QList<Inhibition> &inhibitions, const QString &source);

    KIOTShared::Entities::Sensor *m_suspendSensor = nullptr;
    KIOTShared::Entities::Sensor *m_screenSensor = nullptr;
    QDBusServiceWatcher *m_watcher = nullptr;
    bool m_legacy = false;
    quint64 m_request = 0; // replies of older requests are dropped
    bool m_published = false;
    QList<Inhibition> m_last;
};
