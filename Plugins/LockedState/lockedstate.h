// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <QObject>
#include <KIOTShared/kiotshared.h>
using KIOTShared::Entities::Lock;

class LockedState : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE LockedState(QObject *parent);

private Q_SLOTS:
    void screenLockedChanged(bool active);
    void stateChangeRequested(bool state);

private:
    Lock *m_locked;
    bool m_activeChangedSeen = false;
};
