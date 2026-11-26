// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include <QObject>

#include "lockedstate.h"
#include <QDebug>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>

DECLARE_LOGGER(plugin_logger);

LockedState::LockedState(QObject *parent)
    : QObject(parent)
{
    m_locked = new Lock(this);
    m_locked->setId("locked");
    m_locked->setName("Locked State");
    // why am I used freedesktop here, and logind later.... I don't know
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.ScreenSaver"),
                                          QStringLiteral("/ScreenSaver"),
                                          QStringLiteral("org.freedesktop.ScreenSaver"),
                                          QStringLiteral("ActiveChanged"),
                                          this,
                                          SLOT(screenLockedChanged(bool)));
    connect(m_locked, &Lock::stateChangeRequested, this, &LockedState::stateChangeRequested);

    // the screen locker may not be up yet
    auto isLocked = QDBusMessage::createMethodCall("org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver", "GetActive");
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(isLocked), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<bool> reply = *call;
        if (reply.isError()) {
            // an error reply carries the error text as its argument, which isn't the locked state
            qCWarning(plugin_logger) << "Cannot read whether the screen is locked:" << reply.error().message();
            return;
        }
        // ActiveChanged arrived first, it is newer than this reply
        if (!m_activeChangedSeen) {
            m_locked->setState(reply.value());
        }
    });
}

void LockedState::screenLockedChanged(bool active)
{
    m_activeChangedSeen = true;
    m_locked->setState(active);
}

void LockedState::stateChangeRequested(bool state)
{
    if (state) {
        QDBusMessage lock =
            QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1/session/auto", "org.freedesktop.login1.Session", "Lock");
        QDBusConnection::systemBus().asyncCall(lock);
    } else {
        QDBusMessage unlock =
            QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1/session/auto", "org.freedesktop.login1.Session", "Unlock");
        QDBusConnection::systemBus().asyncCall(unlock);
    }
}

