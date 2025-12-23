// SPDX-FileCopyrightText: 2025
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <KLocalizedString>
#include <KPluginFactory>
#include <KQuickManagedConfigModule>
#include <KSharedConfig>
#include <QStandardPaths>

#include "kiotsettings.h"

#include <QLoggingCategory>
#include <QFile>
#include <QTextStream>
#include <QVariantMap>
#include <QVariantList>
#include <QRegularExpression>
#include <QDebug>
Q_DECLARE_LOGGING_CATEGORY(ui)
Q_LOGGING_CATEGORY(ui, "kiot.UI.kcm")

class KCMKiot : public KQuickManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(KiotSettings *settings READ settings CONSTANT)
    Q_PROPERTY(QVariantMap configSections READ configSections NOTIFY configSectionsChanged)
    Q_PROPERTY(QVariantList sectionOrder READ sectionOrder NOTIFY sectionOrderChanged)
    
public:
    explicit KCMKiot(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args)
        : KQuickManagedConfigModule(parent, metaData)
        , m_settings(new KiotSettings(KSharedConfig::openConfig("kiotrc", KSharedConfig::CascadeConfig), this))
    {
        Q_UNUSED(args);
        setButtons(Apply | Default);
        qCDebug(ui) << m_settings->host() << m_settings->config()->name();
        
        loadConfigFile();
    }

    KiotSettings *settings() const { return m_settings; }
    
    QVariantMap configSections() const { 
        return m_configSections; 
    }
    QVariantList sectionOrder() const { 
        return m_sectionOrder; 
    }
    
    Q_INVOKABLE void saveConfigValue(const QString &section, const QString &key, const QVariant &value) {
        qCDebug(ui) << "Saving config value:" << section << key << value;
        auto config = KSharedConfig::openConfig("kiotrc", KSharedConfig::CascadeConfig);
        
        if (section.contains("][")) {
            QStringList parts = section.split("][");
            if (parts.size() >= 2) {
                KConfigGroup mainGroup(config, parts[0]);
                KConfigGroup subGroup(&mainGroup, parts[1]);
                writeEntry(subGroup, key, value);
                subGroup.sync();
            }
        } else {
            KConfigGroup group(config, section);
            writeEntry(group, key, value);
            group.sync();
        }
        
        if (m_configSections.contains(section)) {
            QVariantMap sectionMap = m_configSections[section].toMap();
            sectionMap[key] = value;
            m_configSections[section] = sectionMap;
            emit configSectionsChanged();
        }
    }
    
    Q_INVOKABLE void saveNestedConfigValue(const QString &mainSection, const QString &subSection, 
                                          const QString &key, const QVariant &value) {
        qCDebug(ui) << "Saving nested config value:" << mainSection << subSection << key << value;
        auto config = KSharedConfig::openConfig("kiotrc", KSharedConfig::CascadeConfig);
        KConfigGroup mainGroup(config, mainSection);
        KConfigGroup subGroup(&mainGroup, subSection);
        
        writeEntry(subGroup, key, value);
        subGroup.sync();
        
        QString fullSection = QString("%1][%2").arg(mainSection).arg(subSection);
        if (m_configSections.contains(fullSection)) {
            QVariantMap sectionMap = m_configSections[fullSection].toMap();
            sectionMap[key] = value;
            m_configSections[fullSection] = sectionMap;
            emit configSectionsChanged();
        }
    }
    
    Q_INVOKABLE QVariant getConfigValue(const QString &section, const QString &key, const QVariant &defaultValue = QVariant()) {
        qCDebug(ui) << "Getting config value:" << section << key << defaultValue;
        auto config = KSharedConfig::openConfig("kiotrc", KSharedConfig::CascadeConfig);
        
        if (section.contains("][")) {
            QStringList parts = section.split("][");
            if (parts.size() >= 2) {
                KConfigGroup mainGroup(config, parts[0]);
                KConfigGroup subGroup(&mainGroup, parts[1]);
                return readEntry(subGroup, key, defaultValue);
            }
        }
        
        KConfigGroup group(config, section);
        return readEntry(group, key, defaultValue);
    }
    
    Q_INVOKABLE void deleteNestedConfig(const QString &mainSection, const QString &subSection) {
        qCDebug(ui) << "Deleting nested config:" << mainSection << subSection;
        auto config = KSharedConfig::openConfig("kiotrc", KSharedConfig::CascadeConfig);
        KConfigGroup mainGroup(config, mainSection);
    
        mainGroup.deleteGroup(subSection);
        mainGroup.sync();
    
        QString fullSection = QString("%1][%2").arg(mainSection).arg(subSection);
        if (m_configSections.contains(fullSection)) {
            m_configSections.remove(fullSection);
            emit configSectionsChanged();
        }
    }

    
private:
    void writeEntry(KConfigGroup &group, const QString &key, const QVariant &value) {
        if (value.typeId() == QMetaType::Bool) {
            group.writeEntry(key, value.toBool());
        } else if (value.typeId() == QMetaType::Int || value.typeId() == QMetaType::LongLong) {
            group.writeEntry(key, value.toInt());
        } else if (value.typeId() == QMetaType::Double) {
            group.writeEntry(key, value.toDouble());
        } else {
            group.writeEntry(key, value.toString());
        }
    }
    
    QVariant readEntry(KConfigGroup &group, const QString &key, const QVariant &defaultValue) {
        if (defaultValue.typeId() == QMetaType::Bool) {
            return group.readEntry(key, defaultValue.toBool());
        } else if (defaultValue.typeId() == QMetaType::Int || defaultValue.typeId() == QMetaType::LongLong) {
            return group.readEntry(key, defaultValue.toInt());
        } else if (defaultValue.typeId() == QMetaType::Double) {
            return group.readEntry(key, defaultValue.toDouble());
        } else {
            return group.readEntry(key, defaultValue.toString());
        }
    }
    
    void loadConfigFile() {
        m_configSections.clear();
        m_sectionOrder.clear();
        
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/kiotrc";
        QFile file(configPath);
        
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(ui) << "Could not open config file:" << configPath;
            loadFromKConfig();
            return;
        }
        
        QTextStream in(&file);
        QString currentSection;
        QString currentSubSection;
        QVariantMap currentSectionData;
        QRegularExpression mainSectionRegex("^\\[([^\\]]+)\\]$");
        QRegularExpression nestedSectionRegex("^\\[([^\\]]+)\\]\\[([^\\]]+)\\]$");
        QRegularExpression keyValueRegex("^([^=]+)=(.*)$");
        
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith("#") || line.startsWith(";")) {
                continue;
            }
            
            QRegularExpressionMatch nestedSectionMatch = nestedSectionRegex.match(line);
            if (nestedSectionMatch.hasMatch()) {
                if (!currentSection.isEmpty()) {
                    QString fullSectionName = currentSubSection.isEmpty() 
                        ? currentSection 
                        : QString("%1][%2").arg(currentSection).arg(currentSubSection);
                    m_configSections[fullSectionName] = currentSectionData;
                    m_sectionOrder.append(fullSectionName);
                    currentSectionData.clear();
                }
                
                currentSection = nestedSectionMatch.captured(1);
                currentSubSection = nestedSectionMatch.captured(2);
                continue;
            }
            
            QRegularExpressionMatch mainSectionMatch = mainSectionRegex.match(line);
            if (mainSectionMatch.hasMatch()) {
                if (!currentSection.isEmpty()) {
                    QString fullSectionName = currentSubSection.isEmpty() 
                        ? currentSection 
                        : QString("%1][%2").arg(currentSection).arg(currentSubSection);
                    m_configSections[fullSectionName] = currentSectionData;
                    m_sectionOrder.append(fullSectionName);
                    currentSectionData.clear();
                }
                
                currentSection = mainSectionMatch.captured(1);
                currentSubSection = QString();
                continue;
            }
            
            QRegularExpressionMatch kvMatch = keyValueRegex.match(line);
            if (kvMatch.hasMatch() && !currentSection.isEmpty()) {
                QString key = kvMatch.captured(1).trimmed();
                QString value = kvMatch.captured(2).trimmed();
                if (value == "true") {
                    currentSectionData[key] = true;
                } else if (value == "false") {
                    currentSectionData[key] = false;
                } else {
                    currentSectionData[key] = value;
                }
            qCDebug(ui) << "After parsing - key:" << key << "value:" << currentSectionData[key] << "type:" << currentSectionData[key].typeName();
      
            }
        }
        
        if (!currentSection.isEmpty()) {
            QString fullSectionName = currentSubSection.isEmpty() 
                ? currentSection 
                : QString("%1][%2").arg(currentSection).arg(currentSubSection);
            m_configSections[fullSectionName] = currentSectionData;
            m_sectionOrder.append(fullSectionName);
        }
        
        if (m_sectionOrder.contains("general")) {
            m_sectionOrder.removeAll("general");
            m_sectionOrder.prepend("general");
        }
        
        groupNestedSections();
        
        for (const QVariant &sectionVar : m_sectionOrder) {
            QString sectionStr = sectionVar.toString();
            QVariant sectionData = m_configSections.value(sectionStr);
            qCDebug(ui) << "Section:" << sectionStr << "type:" << sectionData.typeName() << "keys:" << sectionData.toMap().keys();
        }
        
        emit configSectionsChanged();
        emit sectionOrderChanged();
    }
    
    void groupNestedSections() {
        QVariantList newOrder;
        QVariantMap newSections = m_configSections;
        
        if (m_sectionOrder.contains("general")) {
            newOrder.append("general");
        }
        
        for (const QVariant &sectionVar : m_sectionOrder) {
            QString section = sectionVar.toString();
            
            if (section == "general" || 
                section.startsWith("Scripts][") || 
                section.startsWith("Shortcuts][")) {
                continue;
            }
            newOrder.append(section);
        }
        
        QVariantMap scriptsData;
        for (const QVariant &sectionVar : m_sectionOrder) {
            QString section = sectionVar.toString();
            if (section.startsWith("Scripts][")) {
                scriptsData[section] = m_configSections[section];
                newSections.remove(section);
            }
        }
        
        newOrder.append("Scripts");
        newSections["Scripts"] = scriptsData;
        
        QVariantMap shortcutsData;
        for (const QVariant &sectionVar : m_sectionOrder) {
            QString section = sectionVar.toString();
            if (section.startsWith("Shortcuts][")) {
                shortcutsData[section] = m_configSections[section];
                newSections.remove(section);
            }
        }
        
        newOrder.append("Shortcuts");
        newSections["Shortcuts"] = shortcutsData;
        
        m_sectionOrder = newOrder;
        m_configSections = newSections;
    }
    
    void loadFromKConfig() {
        auto config = KSharedConfig::openConfig("kiotrc", KSharedConfig::CascadeConfig);
        
        const auto groupList = config->groupList();
        
        for (const QString &groupName : groupList) {
            KConfigGroup group(config, groupName);
            QVariantMap sectionData;
            
            const auto keyList = group.keyList();
            for (const QString &key : keyList) {
                QString valueStr = group.readEntry(key, QString());
                
                if (valueStr == "true") {
                    sectionData[key] = true;
                } else if (valueStr == "false") {
                    sectionData[key] = false;
                } else {
                    sectionData[key] = valueStr;
                }
            }
            
            const auto subGroupList = group.groupList();
            for (const QString &subGroupName : subGroupList) {
                KConfigGroup subGroup(&group, subGroupName);
                QString fullSectionName = QString("%1][%2").arg(groupName).arg(subGroupName);
                QVariantMap subSectionData;
                
                const auto subKeyList = subGroup.keyList();
                for (const QString &key : subKeyList) {
                    QString valueStr = subGroup.readEntry(key, QString());
                    
                    QString lowerValue = valueStr.toLower();
                    if (lowerValue == "true" || lowerValue == "false") {
                        subSectionData[key] = (lowerValue == "true");
                    } else {
                        subSectionData[key] = valueStr;
                    }
                }
                
                m_configSections[fullSectionName] = subSectionData;
                m_sectionOrder.append(fullSectionName);
            }
            
            if (!keyList.isEmpty()) {
                m_configSections[groupName] = sectionData;
                m_sectionOrder.append(groupName);
            }
        }
        
        // Always ensure general is first
        if (m_sectionOrder.contains("general")) {
            m_sectionOrder.removeAll("general");
            m_sectionOrder.prepend("general");
        }
        
        groupNestedSections();
        
        emit configSectionsChanged();
        emit sectionOrderChanged();
    }

signals:
    void configSectionsChanged();
    void sectionOrderChanged();

private:
    KiotSettings *m_settings;
    QVariantMap m_configSections;
    QVariantList m_sectionOrder;
};

K_PLUGIN_CLASS_WITH_JSON(KCMKiot, "kcm_kiot.json")

#include "kcm_kiot.moc"