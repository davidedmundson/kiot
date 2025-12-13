// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class Entity : public QObject
{
    Q_OBJECT
public:
    void setId(const QString &newId);
    QString id() const;

    void setName(const QString &newName);
    QString name() const;

    void setHaIcon(const QString &newHaIcon);
    QString haIcon() const;

    void setDiscoveryConfig(const QString &key, const QVariant &value);

    Entity(QObject *parent);
    QString hostname() const;
    QString baseTopic() const;

protected:
    /**
     * Called on MQTT connect, it may be called more than once
     */
    virtual void init();
    void sendRegistration();
    void setHaType(const QString &newHaType);
    QString haType() const;

private:
    QString m_id;
    QString m_name;
    QString m_haIcon = "";
    QString m_haType;
    QVariantMap m_haConfig;
};
