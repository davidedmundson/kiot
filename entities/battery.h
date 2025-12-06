// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include "entity.h"




class Battery : public Entity
{
    
public:
    Battery(QObject *parent = nullptr);

    void setState(const int &state);
    int getState();
    void setAttributes(const QVariantMap &attrs);
    QVariantMap getAttributes();
protected:
    void init() override;

private:
    int m_state;
    QVariantMap m_attributes;

    void publishState();
    void publishAttributes();
};