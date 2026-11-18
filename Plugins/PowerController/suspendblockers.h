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
 * @brief Sensor "Suspend blockers": what currently keeps the session from suspending.
 *
 * Reads PowerDevil's PolicyAgent, which collects the inhibitions of applications (org.freedesktop.PowerManagement,
 * org.freedesktop.ScreenSaver, ...) and the "block" inhibitors other processes hold in logind
 * (e.g. `systemd-inhibit --what=sleep`).
 *
 * State: the number of active inhibitions that block sleep. Attributes: `summary` ("who: why; ..." or "none"),
 * `blockers` (who, why, what of each), `screen_inhibitions` (active inhibitions that only keep the screen on, no
 * suspend blocker) and `source`.
 *
 * Plasma >= 6.5: the ActiveInhibitions property, updated on PropertiesChanged. Older PowerDevil: the deprecated
 * ListInhibitions method and InhibitionsChanged signal, which don't say what an inhibition covers, so every active
 * inhibition counts as a blocker there. Without PowerDevil on the session bus the state is unknown (payload "None").
 */
class SuspendBlockers : public QObject
{
    Q_OBJECT
public:
    explicit SuspendBlockers(QObject *parent = nullptr);
    ~SuspendBlockers() override;

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

    KIOTShared::Entities::Sensor *m_sensor = nullptr;
    QDBusServiceWatcher *m_watcher = nullptr;
    bool m_legacy = false;
    quint64 m_request = 0; // replies of older requests are dropped
    bool m_published = false;
    QList<Inhibition> m_last;
};
