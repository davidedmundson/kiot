// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "entity.h"

class Text : public Entity
{
    Q_OBJECT
public:
    explicit Text(QObject *parent = nullptr);

    void init() override;

    void setState(const QString &text);
    QString state() const
    {
        return m_text;
    }

Q_SIGNALS:
    void stateChangeRequested(const QString &text);

private:
    QString m_text;
};
