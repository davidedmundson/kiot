// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include "entity.h"

class Lock : public Entity
{
    Q_OBJECT
public:
    Lock(QObject *parent = nullptr);
    void setState(bool state);
    void setAttributes(const QVariantMap &attrs);
    bool state()
    {
        return m_state;
    };
    QVariantMap getAttributes()
    {
        return m_attributes;
    }
Q_SIGNALS:
    void stateChangeRequested(bool state);

protected:
    void init() override;

private:
    bool m_state = false;
    QVariantMap m_attributes;
    void publishAttributes();
};