// SPDX-FileCopyrightText: 2025
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "core/core.h"
#include "settingsmanager.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QVariantMap>
#include <QStandardPaths>
#include <QFile>
#include <QProcess>
#include <QApplication>
#include <QDateTime>

DEFINE_LOGGER(settings_sm, UI.SettingsManager)

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
    loadConfigFile();
    qCDebug(settings_sm) << "SettingsManager initialized with KSharedConfig";
}

QVariantMap SettingsManager::configSections() const
{
    return m_configSections;
}

QVariantList SettingsManager::sectionOrder() const
{
    return m_sectionOrder;
}



void SettingsManager::saveConfigValue(const QString &section, const QString &key, const QVariant &value)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    KConfigGroup group(config, section);
    
    // Lagre basert på type
    if (value.typeId() == QMetaType::Bool) {
        group.writeEntry(key, value.toBool());
    } else if (value.typeId() == QMetaType::Int) {
        group.writeEntry(key, value.toInt());
    } else {
        group.writeEntry(key, value.toString());
    }
    config->sync();

    // Oppdater cache
    QVariantMap sectionMap = m_configSections[section].toMap();
    sectionMap[key] = value;
    m_configSections[section] = sectionMap;
    emit configSectionsChanged();
}
void SettingsManager::saveNestedConfigValue(const QString &mainSection, const QString &subSection, const QString &key, const QVariant &value)
{
    // 1. Skriv direkte til disk via KConfig (uten å slette grupper)
    KSharedConfigPtr config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    KConfigGroup parentGroup(config.data(), mainSection);
 
    KConfigGroup subGroup(&parentGroup, subSection);
    qCDebug(settings_sm) << "Saving nested config value:" << mainSection << subSection << key << value;
    if (value.typeId() == QMetaType::Bool) {
        subGroup.writeEntry(key, value.toBool());
    } else if (value.typeId() == QMetaType::Int) {
        subGroup.writeEntry(key, value.toInt());
    } else {
        subGroup.writeEntry(key, value.toString());
    }
    config->sync();

    // 2. Oppdater cachen i tråd med hvordan groupNestedSections bygger den ("Scripts" -> "launch_chrome" -> key)
    QVariantMap mainMap = m_configSections[mainSection].toMap();
    QVariantMap subMap = mainMap[subSection].toMap();
    
    subMap[key] = value;
    mainMap[subSection] = subMap;
    m_configSections[mainSection] = mainMap;

    emit configSectionsChanged();
}


QVariant SettingsManager::getConfigValue(const QString &section, const QString &key, const QVariant &defaultValue) const
{
    KSharedConfigPtr config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    KConfigGroup group(config, section);

    if (defaultValue.typeId() == QMetaType::Bool) {
        return group.readEntry(key, defaultValue.toBool());
    } else if (defaultValue.typeId() == QMetaType::Int) {
        return group.readEntry(key, defaultValue.toInt());
    }
    return group.readEntry(key, defaultValue.toString());
}

void SettingsManager::deleteNestedConfig(const QString &mainSection, const QString &subSection)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    
    // 1. Åpne foreldregruppen først
    KConfigGroup parentGroup(config.data(), mainSection);
    parentGroup.deleteGroup(subSection);
    
    config->sync();

    // 3. Fjern fra lokal cache og oppdater QML
    QString fullSection = mainSection + "/" + subSection;
    if (m_configSections.contains(fullSection)) {
        m_configSections.remove(fullSection);
    } else {
        // Fallback hvis cachen er lagret på et annet nivå
        QVariantMap mainMap = m_configSections[mainSection].toMap();
        if (mainMap.contains(subSection)) {
            mainMap.remove(subSection);
            m_configSections[mainSection] = mainMap;
        }
    }
    
    // Kjører groupNestedSections på nytt for å rydde opp i kartet og rekkefølgen
    groupNestedSections();
    emit configSectionsChanged();
}

void SettingsManager::applySettings()
{
    qCDebug(settings_sm) << "Settings applied, restarting " + QString(PROJECT_NAME);
    QProcess::startDetached(QString(PROJECT_NAME), QStringList());
    QApplication::quit();
}

void SettingsManager::restoreDefaults()
{
    qCDebug(settings_sm) << "Restoring default settings";
    setHost("homeassistant.local");
    setPort(1883);
    setUser("");
    setPassword("");
    setDiscoveryPrefix("homeassistant");
    saveConfigValue("general", "useSSL", false);
    saveConfigValue("general", "autostart", true);
    
    QProcess::startDetached(QString(PROJECT_NAME), QStringList());
    QApplication::quit();
}

// MQTT settings
QString SettingsManager::getHost() const
{
    return getConfigValue("general", "host", "").toString();
}

void SettingsManager::setHost(const QString &host)
{
    saveConfigValue("general", "host", host);
}

int SettingsManager::getPort() const
{
    return getConfigValue("general", "port", 1883).toInt();
}

void SettingsManager::setPort(int port)
{
    saveConfigValue("general", "port", port);
}

QString SettingsManager::getUser() const
{
    return getConfigValue("general", "user", "").toString();
}

void SettingsManager::setUser(const QString &user)
{
    saveConfigValue("general", "user", user);
}

QString SettingsManager::getPassword() const
{
    return getConfigValue("general", "password", "").toString();
}

void SettingsManager::setPassword(const QString &password)
{
    saveConfigValue("general", "password", password);
}

QString SettingsManager::getDiscoveryPrefix() const
{
    return getConfigValue("general", "discoveryprefix", "homeassistant").toString();
}

void SettingsManager::setDiscoveryPrefix(const QString &prefix)
{
    saveConfigValue("general", "discoveryprefix", prefix);
}

void SettingsManager::loadConfigFile()
{
    m_configSections.clear();
        m_sectionOrder.clear();

    KSharedConfigPtr config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    
    // Hent alle hovedgrupper i konfigurasjonsfilen
    QStringList groupNames = config->groupList();

    for (const QString &groupName : groupNames) {
        KConfigGroup group(config, groupName);
        
        // Sjekk om gruppen har undergrupper (f.eks. [Scripts][launch_chrome])
        QStringList subGroups = group.groupList();
        if (!subGroups.isEmpty() && (groupName == "Scripts" || groupName == "Shortcuts"|| groupName == "CustomSensors")) {
            for (const QString &subName : subGroups) {
                KConfigGroup subGroup(&group, subName);
                QString fullSectionKey = groupName + "/" + subName;
                QVariantMap subData;

                QStringList keys = subGroup.keyList();
                for (const QString &key : keys) {
                    QString valStr = subGroup.readEntry(key, QString());
                    // Enkel typekonvertering
                    if (valStr.compare("true", Qt::CaseInsensitive) == 0) {
                        subData[key] = true;
                    } else if (valStr.compare("false", Qt::CaseInsensitive) == 0) {
                        subData[key] = false;
                    } else {
                        bool ok;
                        int intVal = valStr.toInt(&ok);
                        if (ok) {
                            subData[key] = intVal;
                        } else {
                            subData[key] = valStr;
                        }
                    }
                }
                if (!subData.isEmpty()) {
                    m_configSections[fullSectionKey] = subData;
                    if (!m_sectionOrder.contains(fullSectionKey)) {
                        m_sectionOrder.append(fullSectionKey);
                    }
                }
            }
        } else {
            // Standard gruppe uten undergrupper (f.eks. [Main])
            QVariantMap sectionData;
            QStringList keys = group.keyList();
            for (const QString &key : keys) {
                QString valStr = group.readEntry(key, QString());
                if (valStr.compare("true", Qt::CaseInsensitive) == 0) {
                    sectionData[key] = true;
                } else if (valStr.compare("false", Qt::CaseInsensitive) == 0) {
                    sectionData[key] = false;
                } else {
                    bool ok;
                    int intVal = valStr.toInt(&ok);
                    if (ok) {
                        sectionData[key] = intVal;
                    } else {
                        sectionData[key] = valStr;
                    }
                }
            }
            if (!sectionData.isEmpty()) {
                m_configSections[groupName] = sectionData;
                if (!m_sectionOrder.contains(groupName)) { // Standard append
                    m_sectionOrder.append(groupName);
                }
            }
        }
    }

    // Sørg for at "general" alltid kommer først
    if (m_sectionOrder.contains("general")) {
        m_sectionOrder.removeAll("general");
        m_sectionOrder.prepend("general");
    }

    // Kjører groupNestedSections for å rydde opp i QML-strukturen
    groupNestedSections();

    emit configSectionsChanged();
    emit sectionOrderChanged();
}

void SettingsManager::loadFromQSettings()
{
    // Beholdes for bakkompatibilitet eller videresendes til loadConfigFile
    loadConfigFile();
}

void SettingsManager::groupNestedSections()
{
   QVariantList newOrder;
    QVariantMap newSections = m_configSections;

    if (m_sectionOrder.contains("general")) {
        newOrder.append("general");
    }

    // Add non-special sections
    for (const QVariant &sectionVar : m_sectionOrder) {
        QString section = sectionVar.toString();
        if (section == "general" || section.startsWith("Scripts") || section.startsWith("Shortcuts")|| section.startsWith("CustomSensors")) {
            continue;
        }
        newOrder.append(section);
    }

    // Group scripts
    QVariantMap scriptsData;
    for (const QVariant &sectionVar : m_sectionOrder) {
        QString section = sectionVar.toString();
        if (section.startsWith("Scripts")) {
            scriptsData[section] = m_configSections[section];
            newSections.remove(section);
        }
    }
    if (!scriptsData.isEmpty()) {
        newOrder.append("Scripts");
        newSections["Scripts"] = scriptsData;
    }else{
        newOrder.append("Scripts");
        
    }

    // Group shortcuts
    QVariantMap shortcutsData;
    for (const QVariant &sectionVar : m_sectionOrder) {
        QString section = sectionVar.toString();
        if (section.startsWith("Shortcuts")) {
            shortcutsData[section] = m_configSections[section];
            newSections.remove(section);
        }
    }
    if (!shortcutsData.isEmpty()) {
        newOrder.append("Shortcuts");
        newSections["Shortcuts"] = shortcutsData;
    }else{
        newOrder.append("Shortcuts");
        
    }
  // Group CustomSensors
    QVariantMap customsensorsData;
    for (const QVariant &sectionVar : m_sectionOrder) {
        QString section = sectionVar.toString();
        if (section.startsWith("CustomSensors")) {
            customsensorsData[section] = m_configSections[section];
            newSections.remove(section);
        }
    }
    if (!customsensorsData.isEmpty()) {
        newOrder.append("CustomSensors");
        newSections["CustomSensors"] = customsensorsData;
    }else{
        newOrder.append("CustomSensors");
        
    }

    m_sectionOrder = newOrder;
    m_configSections = newSections;
}