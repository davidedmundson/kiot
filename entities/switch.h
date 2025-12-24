// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include "entity.h"

class Switch : public Entity
{
    Q_OBJECT
public:
    Switch(QObject *parent = nullptr);
    void setState(bool state);
    bool state()
    {
        return m_state;
    }
Q_SIGNALS:
    void stateChangeRequested(bool state);

protected:
    void init() override;

private:
    bool m_state = false;
};