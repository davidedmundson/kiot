// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include "entity.h"

class Select : public Entity
{
    Q_OBJECT
public:
    explicit Select(QObject *parent = nullptr);

    void setOptions(const QStringList &opts);
    void setState(const QString &state);
    QString state() const;
    QStringList getOptions() const;

protected:
    void init() override;

signals:
    void optionSelected(QString newOption);

private:
    void publishState();

    QString m_state;
    QStringList m_options;
};