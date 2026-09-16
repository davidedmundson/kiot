// SPDX-FileCopyrightText: 2025
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include "Shared/platformhelper.h"

#include <QObject>
#include <QVariantMap>
#include <QVariantList>

class SettingsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap configSections READ configSections NOTIFY configSectionsChanged)
    Q_PROPERTY(QVariantList sectionOrder READ sectionOrder NOTIFY sectionOrderChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);

    QVariantMap configSections() const;
    QVariantList sectionOrder() const;

    Q_INVOKABLE void saveConfigValue(const QString &section, const QString &key, const QVariant &value);
    Q_INVOKABLE void saveNestedConfigValue(const QString &mainSection, const QString &subSection, const QString &key, const QVariant &value);
    Q_INVOKABLE QVariant getConfigValue(const QString &section, const QString &key, const QVariant &defaultValue = QVariant()) const;
    Q_INVOKABLE void deleteNestedConfig(const QString &mainSection, const QString &subSection);
    Q_INVOKABLE void applySettings();
    Q_INVOKABLE void restoreDefaults();

    // MQTT settings
    Q_INVOKABLE QString getHost() const;
    Q_INVOKABLE void setHost(const QString &host);
    Q_INVOKABLE int getPort() const;
    Q_INVOKABLE void setPort(int port);
    Q_INVOKABLE QString getUser() const;
    Q_INVOKABLE void setUser(const QString &user);
    Q_INVOKABLE QString getPassword() const;
    Q_INVOKABLE void setPassword(const QString &password);
    Q_INVOKABLE QString getDiscoveryPrefix() const;
    Q_INVOKABLE void setDiscoveryPrefix(const QString &prefix);

signals:
    void configSectionsChanged();
    void sectionOrderChanged();

private:
    void loadConfigFile();
    void loadFromQSettings();
    void groupNestedSections();

    QVariantMap m_configSections;
    QVariantList m_sectionOrder;
};

#endif // SETTINGSMANAGER_H