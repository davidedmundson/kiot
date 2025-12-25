// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "entity.h"
#include <QString>

class Event : public Entity
{
    Q_OBJECT
    Q_PROPERTY(QString triggerType READ triggerType WRITE setTriggerType NOTIFY triggerTypeChanged)
    Q_PROPERTY(QString triggerSubtype READ triggerSubtype WRITE setTriggerSubtype NOTIFY triggerSubtypeChanged)
    
public:
    explicit Event(QObject *parent = nullptr);
    
    QString triggerType() const;
    void setTriggerType(const QString &type);
    
    QString triggerSubtype() const;
    void setTriggerSubtype(const QString &subtype);
    
    // Available trigger types (for UI/enumeration)
    Q_INVOKABLE QStringList availableTriggerTypes() const;
    Q_INVOKABLE QStringList availableTriggerSubtypes() const;
    
    void init() override;
    void trigger();
    void triggerCustom(const QString &customType);
signals:
    void triggerTypeChanged();
    void triggerSubtypeChanged();
    
private:
    void triggerWithPayload(const QString &payload);
    QString m_triggerType = QStringLiteral("button_short_press");
    QString m_triggerSubtype;
};
