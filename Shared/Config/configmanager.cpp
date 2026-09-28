#include "configmanager.h"
#include "platformhelper.h"

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QFileSystemWatcher>
DEFINE_LOGGER(lcConfig, ConfigManager)


namespace KIOTShared {

namespace Config {

static bool s_coreInstantiated = false;
static ConfigManager* s_coreInstance = nullptr;
ConfigManager::~ConfigManager()
{
    if (this == s_coreInstance) {
        qCInfo(lcConfig) << "Core ConfigManager destruert. Core-låsen er nå frigjort.";
        s_coreInstance = nullptr;
        s_coreInstantiated = false;
    }
}

// 1. Core uten defaults
ConfigManager::ConfigManager(QObject *parent)
    : ConfigManager(ConfigType::Core, QString(), QJsonObject(), parent)
{}

// 2. Core MED defaults (Brukt i main.cpp)
ConfigManager::ConfigManager(const QJsonObject &defaultConfig, QObject *parent)
    : ConfigManager(ConfigType::Core, QString(), defaultConfig, parent)
{}

// 3. Plugin uten defaults (MÅ bruke denne)
ConfigManager::ConfigManager(const QString &moduleName, QObject *parent)
    : ConfigManager(ConfigType::Plugin, moduleName, QJsonObject(), parent)
{}

// 4. Plugin MED defaults (MÅ bruke denne)
ConfigManager::ConfigManager(const QString &moduleName, const QJsonObject &defaultConfig, QObject *parent)
    : ConfigManager(ConfigType::Plugin, moduleName, defaultConfig, parent)
{}

ConfigManager::ConfigManager(ConfigType type, const QString &moduleName, const QJsonObject &defaultConfig, QObject *parent)
    : QObject(parent)
    , m_type(type)
    , m_defaultData(defaultConfig)
{
    if (m_type == ConfigType::Core) {
        if (s_coreInstance != nullptr) {
            qCCritical(lcConfig) << "SIKKERHETSAVBRUDD: Forsøk på å opprette en sekundær Core ConfigManager ble blokkert!";
            m_type = ConfigType::Plugin;
            m_moduleName = "unauthorized_core_attempt";
        } else {
            s_coreInstance = this;
            s_coreInstantiated = true;
            m_moduleName = "Core";
        }
    } else {
        if (moduleName.trimmed().compare("Core", Qt::CaseInsensitive) == 0) {
            qCCritical(lcConfig) << "SIKKERHETSADVARSEL: Plugin prøvde å kalle seg 'Core'. Omdøpes automatisk.";
            m_moduleName = "invalid_plugin_name";
        } else {
            m_moduleName = moduleName;
        }
    }

    setupFilePath(m_type, m_moduleName);

    QFile file(m_filePath);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            m_data = doc.object();
        } else {
            qCCritical(lcConfig) << "Failed at parsing the config file:" << m_filePath  << "Error:" << parseError.errorString();
            emit configParseError(m_filePath, parseError.errorString());
        }
        file.close();
    }

    if (!m_defaultData.isEmpty()) {
        validateAndMergeDefaults();
    }

    m_fileWatcher = new QFileSystemWatcher(this);
    if (!m_filePath.isEmpty()) {
        if (QFile::exists(m_filePath)) {
            m_fileWatcher->addPath(m_filePath);
        }
    }
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &ConfigManager::handleFileChanged);
}

ConfigManager::JsonResult ConfigManager::validateJsonString(const QString &json)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        qCCritical(lcConfig) << "Ugyldig JSON-streng ved posisjon" 
                             << error.offset << ":" << error.errorString();
        return {false, {}};
    }

    if (!doc.isObject()) {
        qCCritical(lcConfig) << "Ugyldig JSON: Rot-elementet er ikke et objekt";
        return {false, {}};
    }

    return {true, doc.object()};
}

void ConfigManager::setDefaultConfig(const QJsonObject &defaultConfig)
{
    m_defaultData = defaultConfig;
    validateAndMergeDefaults();
}

QString ConfigManager::sanitizeModuleName(const QString &rawName) const
{
    QString clean = rawName.toLower().trimmed();
    clean.remove('/');
    clean.remove('\\');
    clean.remove("..");
    return clean;
}

void ConfigManager::setupFilePath(ConfigType type, const QString &moduleName)
{
    QString baseConfigDir = PlatformHelper::configDirPath();
    
    QDir dir(baseConfigDir);
    if (!dir.exists()) {
        QDir().mkpath(baseConfigDir);
    }

    if (type == ConfigType::Core) {
        m_filePath =  baseConfigDir + "/" +PlatformHelper::getProjectName();// PlatformHelper::configFilePath();
    } else {
        QString cleanName = sanitizeModuleName(moduleName);
        if (cleanName.isEmpty() || cleanName == "core") {
            cleanName = "unnamed_plugin";
        }

        QString pluginsDir = baseConfigDir + "/plugins";
        QDir().mkpath(pluginsDir);
        m_filePath = pluginsDir + "/" + cleanName;
    }
}
QString ConfigManager::filePath() { return m_filePath;}


QJsonValue ConfigManager::value(const QString &keyPath, const QJsonValue &defaultValue) const
{
    QJsonValue val = getRawValue(m_data, keyPath);
    if (!val.isUndefined() && !val.isNull()) {
        return val;
    }

    QJsonValue defaultVal = getRawValue(m_defaultData, keyPath);
    if (!defaultVal.isUndefined() && !defaultVal.isNull()) {
        return defaultVal;
    }

    return defaultValue;
}

QJsonValue ConfigManager::getRawValue(const QJsonObject &source, const QString &keyPath) const
{
    QStringList parts = keyPath.split('/');
    QJsonObject currentObj = source;

    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!currentObj.contains(parts[i]) || !currentObj.value(parts[i]).isObject()) {
            return QJsonValue();
        }
        currentObj = currentObj.value(parts[i]).toObject();
    }

    return currentObj.value(parts.last());
}

void ConfigManager::setValue(const QString &keyPath, const QJsonValue &value)
{
    setRawValue(m_data, keyPath, value);
    m_isSaving = true;
    
    QFile file(m_filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(m_data);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    } else {
        qCWarning(lcConfig) << "Kunne ikke skrive konfigurasjon til fil:" << m_filePath;
        emit configWriteError(m_filePath, file.errorString());
    }
    m_isSaving = false; 
    emit configChanged(keyPath, value);
}

void ConfigManager::setRawValue(QJsonObject &target, const QString &keyPath, const QJsonValue &value)
{
    QStringList parts = keyPath.split('/');
    if (parts.isEmpty()) return;

    if (parts.size() == 1) {
        target.insert(parts.first(), value);
    } else {
        QVector<QJsonObject> stack;
        QJsonObject currentObj = target;

        for (int i = 0; i < parts.size() - 1; ++i) {
            stack.append(currentObj);
            currentObj = currentObj.value(parts[i]).toObject();
        }

        currentObj.insert(parts.last(), value);

        for (int i = parts.size() - 2; i >= 0; --i) {
            QJsonObject parentObj = stack[i];
            parentObj.insert(parts[i], currentObj);
            currentObj = parentObj;
        }

        target = currentObj;
    }
}

void ConfigManager::validateAndMergeDefaults()
{
    if (m_defaultData.isEmpty()) return;

    if (mergeObjects(m_data, m_defaultData)) {
        QFile file(m_filePath);
        m_isSaving = true;
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QJsonDocument doc(m_data);
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();
        }else{
            qCDebug(lcConfig) << "Failed to open file for writing:" << file.errorString();
            emit configWriteError(m_filePath, file.errorString());
        }
        m_isSaving = false; 

    }
}

bool ConfigManager::mergeObjects(QJsonObject &target, const QJsonObject &defaults)
{
    bool modified = false;

    for (auto it = defaults.begin(); it != defaults.end(); ++it) {
        QString key = it.key();
        QJsonValue defaultValue = it.value();

        if (!target.contains(key)) {
            target.insert(key, defaultValue);
            modified = true;
        } else if (defaultValue.isObject() && target.value(key).isObject()) {
            QJsonObject childObj = target.value(key).toObject();
            if (mergeObjects(childObj, defaultValue.toObject())) {
                target.insert(key, childObj);
                modified = true;
            }
        }
    }

    return modified;
}

bool ConfigManager::contains(const QString &keyPath) const
{
    QJsonValue val = getRawValue(m_data, keyPath);
    return !val.isUndefined() && !val.isNull();
}

void ConfigManager::remove(const QString &keyPath)
{
    m_data.remove(keyPath);
    QFile file(m_filePath);
    m_isSaving = true;
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(m_data);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }else{
        qCDebug(lcConfig) << "Failed to open file for writing:" << file.errorString();
        emit configWriteError(m_filePath, file.errorString());
    }
    m_isSaving = false; 

}

void ConfigManager::resetToDefaults()
{
    m_data = m_defaultData;
    QFile file(m_filePath);
    m_isSaving = true;
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(m_data);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }else{
        qCDebug(lcConfig) << "Failed to open file for writing:" << file.errorString();
        emit configWriteError(m_filePath, file.errorString());
    }
    m_isSaving = false; 
}


void ConfigManager::handleFileChanged(const QString &path)
{
    if (!m_fileWatcher->files().contains(path)) {
        if (QFile::exists(path)) {
            m_fileWatcher->addPath(path);
        }
    }

    if (m_isSaving) {
        return;
    }

    qCInfo(lcConfig) << "Config file changed externally, reloading:" << path;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        file.close();

        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject newData = doc.object();
            //Here we update with new info from outside edits before sending signal
            m_data = newData;

            emit configChanged("*", QJsonValue()); 
        } else {
            qCWarning(lcConfig) << "Failed to parse JSON file:" << parseError.errorString();
            emit configParseError(path, parseError.errorString());
        }
    }
}


}
}