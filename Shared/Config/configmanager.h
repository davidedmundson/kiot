#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include "KIOTShared/kiotshared_export.h"

class QFileSystemWatcher; // Forward declaration

namespace KIOTShared {
namespace Config {

class KIOT_SHARED_EXPORT ConfigManager : public QObject
{
    Q_OBJECT

public:
    struct JsonResult {
        bool success;
        QJsonObject data;
    };

    // --- CORE-ONLY CONSTRUCTORS ---
    // Core without defaults (Can ONLY be called from main/system)
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager() override;
    // Core WITH defaults (Used in main.cpp)
    explicit ConfigManager(const QJsonObject &defaultConfig, QObject *parent = nullptr);

    // --- PLUGIN CONSTRUCTORS ---
    // Plugin configuration (MUST specify module name!)
    explicit ConfigManager(const QString &moduleName, QObject *parent = nullptr);

    // Plugin configuration WITH default data (MUST specify module name!)
    explicit ConfigManager(const QString &moduleName, const QJsonObject &defaultConfig, QObject *parent = nullptr);

    // Static validation of JSON string
    static JsonResult validateJsonString(const QString &json);

    // Methods for reading and writing
    QJsonValue value(const QString &keyPath, const QJsonValue &defaultValue = QJsonValue()) const;
    void setValue(const QString &keyPath, const QJsonValue &value);
    bool contains(const QString &keyPath) const;
    void remove(const QString &keyPath);
    void resetToDefaults();
    QString filePath();
    void setDefaultConfig(const QJsonObject &defaultConfig);

Q_SIGNALS:
    void configChanged(const QString &keyPath, const QJsonValue &newValue);
    void configParseError(const QString &filePath, const QString &errorString); 
    void configWriteError(const QString &filePath, const QString &errorString); 

private:
    enum class ConfigType {
        Core,
        Plugin
    };

    // MASTER CONSTRUCTOR IS NOW PRIVATE!
    // No external files (neither main nor plugins) can call this directly.
    explicit ConfigManager(ConfigType type, const QString &moduleName, const QJsonObject &defaultConfig, QObject *parent);

    ConfigType m_type;
    QString m_moduleName;
    QString m_filePath;
    QJsonObject m_data;
    QJsonObject m_defaultData;

    QFileSystemWatcher *m_fileWatcher = nullptr;
    bool m_isSaving = false; // To avoid triggering signals on self-saves
    
    void setupFilePath(ConfigType type, const QString &moduleName);
    QString sanitizeModuleName(const QString &rawName) const;
    void validateAndMergeDefaults();
    bool mergeObjects(QJsonObject &target, const QJsonObject &defaults);

    QJsonValue getRawValue(const QJsonObject &source, const QString &keyPath) const;
    void setRawValue(QJsonObject &target, const QString &keyPath, const QJsonValue &value);
    // Writes m_data to m_filePath and watches the file from then on
    void writeFile();

private Q_SLOTS:
    void handleFileChanged(const QString &path);
};

}
}