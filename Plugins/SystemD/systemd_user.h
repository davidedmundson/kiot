// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <KIOTShared/kiotshared.h>

using KIOTShared::Entities::Switch;
using KIOTShared::PlatformHelper;



class SystemDWatcher : public QObject
{
    Q_OBJECT
public:
    explicit SystemDWatcher(QObject *parent = nullptr);
    ~SystemDWatcher() = default;

    bool ensureConfig();

    // Unit name from a unit's D-Bus object path, e.g. "app_40foo_2eservice" -> "app@foo.service"
    static QString pathToUnitName(const QString &path);

private Q_SLOTS:
    void onUnitPropertiesChanged(const QString &interface, const QVariantMap &changedProps, const QStringList &invalidatedProps, const QDBusMessage &msg);
    void performInit();

private:
    QHash<QString, Switch *> m_serviceSwitches;
    QDBusInterface *m_systemdUser = nullptr;
    QString sanitizeServiceId(const QString &svc);
    QStringList listUserServices() const;

    bool m_initialized = false;
};
