// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include "entity.h"

class Number : public Entity
{
    Q_OBJECT
public:
    Number(QObject *parent = nullptr);
    void setValue(int value);
    int value();
    // Optional customization for integrations before init()
    void setRange(int min, int max, int step = 1, const QString &unit = "%");

protected:
    void init() override;

Q_SIGNALS:
    void valueChangeRequested(int value);

private:
    int m_value = 0;
    int m_min = 0;
    int m_max = 100;
    int m_step = 1;
    QString m_unit = "%";
};
