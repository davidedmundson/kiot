// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/lock.h"
#include <QObject>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>

#include <QCoreApplication>

class LockedState : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE LockedState(QObject *parent);

private Q_SLOTS:
    void screenLockedChanged(bool active);
    void stateChangeRequested(bool state);

private:
    Lock m_locked;
};

LockedState::LockedState(QObject *parent)
    : QObject(parent)
{
    m_locked.setId("locked");
    m_locked.setName("Locked");

    // why am I used freedesktop here, and logind later.... I don't know
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.ScreenSaver"),
                                          QStringLiteral("/ScreenSaver"),
                                          QStringLiteral("org.freedesktop.ScreenSaver"),
                                          QStringLiteral("ActiveChanged"),
                                          this,
                                          SLOT(screenLockedChanged(bool)));

    connect(&m_locked, &Lock::stateChangeRequested, this, &LockedState::stateChangeRequested);

    auto isLocked = QDBusMessage::createMethodCall("org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver", "GetActive");
    auto pendingCall = QDBusConnection::sessionBus().asyncCall(isLocked);
    pendingCall.waitForFinished();
    const bool locked = pendingCall.reply().arguments().at(0).toBool();
    m_locked.setState(locked);
}

void LockedState::screenLockedChanged(bool active)
{
    m_locked.setState(active);
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

void registerLockedState()
{
    new LockedState(qApp);
}

REGISTER_INTEGRATION("LockedState", registerLockedState, true)
#include "lockedstate.moc"
