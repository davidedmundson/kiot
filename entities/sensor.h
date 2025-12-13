// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include "entity.h"

class Sensor : public Entity
{
    Q_OBJECT
public:
    Sensor(QObject *parent = nullptr);

    void setState(const QString &state);
    void setAttributes(const QVariantMap &attrs);
    QString state()
    {
        return m_state;
    };
    QVariantMap attributes()
    {
        return m_attributes;
    };

protected:
    void init() override;

private:
    QString m_state;
    QVariantMap m_attributes;

    void publishState();
    void publishAttributes();
};