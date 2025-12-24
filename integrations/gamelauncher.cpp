// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

// TODO look into making this as one select entity instead of one per launcher
#include "core.h"
#include "entities/select.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KSandbox>

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>

#include <QObject>
#include <QString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QVariantMap>
#include <QDirIterator>
#include <QTextStream>


#include <QByteArray>
#include <QApplication>
#include <QProcess>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(gl)
Q_LOGGING_CATEGORY(gl, "integration.GameLauncher")
Q_DECLARE_LOGGING_CATEGORY(steam)
Q_LOGGING_CATEGORY(steam, "integration.GameLauncher.Steam")
Q_DECLARE_LOGGING_CATEGORY(heroic)
Q_LOGGING_CATEGORY(heroic, "integration.GameLauncher.Heroic")

#include <QRegularExpression>
namespace {
    static const QRegularExpression invalidCharRegex("[^a-zA-Z0-9_-]");
}

/**
 * @class GameLauncher
 * @brief Steam game launcher integration for Kiot
 * 
 * @details
 * This integration discovers installed Steam games and creates
 * button entities for launching them directly from Home Assistant.
 * It parses Steam's libraryfolders.vdf and appmanifest files to
 * find all installed games and creates launch commands for them.
 */
class Steam : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a GameLauncher instance
     * @param parent Parent QObject (optional)
     * 
     * @details
     * Initializes the game launcher by checking if Steam is installed,
     * finding the library configuration, and creating button entities
     * for all discovered games.
     */
    explicit Steam(QObject *parent = nullptr)
        : QObject(parent)
    {

        QString libraryConfig = findLibraryConfig();
        if (libraryConfig.isEmpty()) {
            qCWarning(steam) << "Could not find Steam library configuration. GameLauncher integration disabled.";
            return;
        }

        qCDebug(steam) << "Found Steam library config:" << libraryConfig;

        QMap<QString, QString> games = getGamesDirect(libraryConfig);
        if (games.isEmpty()) {
            qCWarning(steam) << "No games found. GameLauncher integration disabled.";
            return;
        }
        ensureConfig(games);

        qCDebug(steam) << "Found" << games.size() << "games";
        m_select = new Select(this);
        m_select->setId("steam_launcher");
        m_select->setName("Steam Launcher");
        m_select->setDiscoveryConfig("icon","mdi:steam")
        connect(m_select, &Select::optionSelected, this, &Steam::onOptionSelected);
        createGameEntities(games);
    }

private slots:
    /**
     * @brief Slot called when a option is selcted 
     * @param option The Steam Game name to launch
     * 
     * @details
     * Launches the specified game using Steam's URI scheme.
     * The game is launched in the background without bringing
     * Steam client to the foreground.
     */
    void onOptionSelected(const QString &option)
    {
        if(option == "Default") return;
        QString gameId = m_gameList.value(option);
        qCDebug(steam) << "Launching game with App ID:" << gameId;
        
        QString launchCommand = QString("xdg-open steam://rungameid/%1").arg(gameId);
        QStringList args = QProcess::splitCommand(launchCommand);  
           
        QString program = args.takeFirst();           
        QProcess *process = new QProcess(this);
        
        process->setProgram(program);
        process->setArguments(args);
        if (KSandbox::isFlatpak()) {
            KSandbox::ProcessContext ctx = KSandbox::makeHostContext(*process);
            process->setProgram(ctx.program);
            process->setArguments(ctx.arguments);
        }
        // TODO, should probably make this detached instead
        process->start();
        
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, gameId, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                        qCDebug(steam) << "Successfully launched game:" << gameId;
                    } else {
                        qCWarning(steam) << "Failed to launch game" << gameId 
                                     << ": exit code" << exitCode;
                    }
                    process->deleteLater();
                });
    }

private:
    /**
     * @brief Sanitizes a game name for use as a config key
     * @param gameName The original game name
     * @return Sanitized string safe for config keys
     */
    QString sanitizeGameName(const QString &gameName)
    {

        QString id = gameName.toLower();
        id.replace(invalidCharRegex, QStringLiteral("_"));
        if (!id.isEmpty() && id[0].isDigit()) {
            id.prepend("game_");
        }
        return id;
    }

    /**
     * @brief Ensures configuration has entries for all discovered games
     * @param games Map of App ID to game name
     * 
     * @details
     * Updates the steam config group with sanitized game names as keys
     * and true/false values indicating whether each game should be exposed.
     * New games default to false (not exposed).
     */
    void ensureConfig(const QMap<QString, QString> &games)
    {
        auto cfg = KSharedConfig::openConfig();
        KConfigGroup grp = cfg->group("steam");
    
        bool configChanged = false;
        
        for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
            const QString &gameName = it.value();
            QString configKey = sanitizeGameName(gameName);
        
            if (!grp.hasKey(configKey)) {
                grp.writeEntry(configKey, false);
                configChanged = true;
                qCDebug(steam) << "Added new steam game to config:" << configKey << "= false";
            }
        }
    
        const QStringList currentKeys = grp.keyList();
    
        for (const QString &configKey : currentKeys) {
            bool gameStillExists = false;
        
            for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
                const QString &gameName = it.value();
                if (sanitizeGameName(gameName) == configKey) {
                    gameStillExists = true;
                    break;
                }
            }
        
            if (!gameStillExists) {
                grp.deleteEntry(configKey);
                configChanged = true;
                qCDebug(steam) << "Removed unavailable game from config:" << configKey;
            }
        }
    
        if (configChanged) {
            cfg->sync();
            qCDebug(steam) << "Configuration updated with current games";
        }
    }


    /**
     * @brief Finds Steam library configuration file
     * @return Path to libraryfolders.vdf if found, empty string otherwise
     * 
     * @details
     * Searches in standard Steam locations first, then falls back
     * to recursive search if not found in standard locations.
     */
    QString findLibraryConfig()
    {
        QStringList standardPaths = {
            QDir::homePath() + "/.local/share/Steam/config/libraryfolders.vdf",
            QDir::homePath() + "/.steam/steam/config/libraryfolders.vdf",
            QDir::homePath() + "/.var/app/com.valvesoftware.Steam/data/Steam/config/libraryfolders.vdf",
            "/home/steam/.local/share/Steam/config/libraryfolders.vdf"
        };

        for (const QString &path : standardPaths) {
            if (QFile::exists(path)) {
                qCDebug(steam) << "Found libraryfolders.vdf in standard location:" << path;
                return path;
            }
        }

        QString steamHome = QDir::homePath() + "/.local/share/Steam";
        if (QDir(steamHome).exists()) {
            QString foundPath = recursiveFind(QDir(steamHome), 0, 3);
            if (!foundPath.isEmpty()) {
                qCDebug(steam) << "Found libraryfolders.vdf via recursive search:" << foundPath;
                return foundPath;
            }
        }

        qCDebug(steam) << "Falling back to limited recursive search from home directory";
        return recursiveFind(QDir(QDir::homePath()), 0, 3);
    }

    /**
     * @brief Simple direct parser for Steam libraryfolders.vdf
     * @param steamConfigPath Path to libraryfolders.vdf
     * @return Map of App ID to game name
     * 
     * @details
     * Directly parses the VDF file without complex nesting logic.
     * Extracts library paths and app IDs, then reads appmanifest
     * files to get game names.
     */
    QMap<QString, QString> getGamesDirect(const QString &steamConfigPath)
    {
        QMap<QString, QString> games;
        
        QFile file(steamConfigPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(steam) << "Failed to open Steam config:" << steamConfigPath;
            return games;
        }
        
        QTextStream in(&file);
        QString currentLibraryPath;
        bool inAppsSection = false;
        int braceDepth = 0;
        int appsBraceDepth = 0;
        
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.trimmed().isEmpty()) continue;
            
            if (line.contains('{')) {
                braceDepth++;
                if (inAppsSection && appsBraceDepth == 0) {
                    appsBraceDepth = braceDepth;
                }
            }
            
            if (line.contains('}')) {
                if (inAppsSection && braceDepth == appsBraceDepth) {
                    inAppsSection = false;
                    appsBraceDepth = 0;
                }
                braceDepth--;
            }
            
            // Look for library path (format: "path"		"/path/to/library")
            if (line.contains("\"path\"\t\t\"")) {
                int startPos = line.indexOf("\"path\"");
                startPos = line.indexOf('\"', startPos + 6); // Skip "path"
                if (startPos != -1) {
                    int endPos = line.indexOf('\"', startPos + 1);
                    if (endPos != -1) {
                        currentLibraryPath = line.mid(startPos + 1, endPos - startPos - 1);
                        qCDebug(steam) << "Found library path:" << currentLibraryPath;
                    }
                }
            }
            
            if (line.contains("\"apps\"")) {
                inAppsSection = true;
                continue;
            }
            
            if (inAppsSection && !currentLibraryPath.isEmpty()) {
                line = line.trimmed();
                if (line.startsWith('\"') && line.count('\"') >= 2) {
                    int firstQuote = line.indexOf('\"');
                    int secondQuote = line.indexOf('\"', firstQuote + 1);
                    if (firstQuote != -1 && secondQuote != -1) {
                        QString appId = line.mid(firstQuote + 1, secondQuote - firstQuote - 1);
                        
                        bool isNumeric = false;
                        appId.toInt(&isNumeric);
                        
                        if (isNumeric && !games.contains(appId)) {
                            QString acfPath = QDir(currentLibraryPath).filePath(
                                QString("steamapps/appmanifest_%1.acf").arg(appId));
                            
                            QFile acfFile(acfPath);
                            if (acfFile.exists() && acfFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                                QTextStream acfIn(&acfFile);
                                QString gameName;
                                
                                while (!acfIn.atEnd()) {
                                    QString acfLine = acfIn.readLine();
                                    if (acfLine.contains("\"name\"\t\t\"")) {
                                        int nameStart = acfLine.indexOf('\"', acfLine.indexOf("\"name\"") + 6);
                                        int nameEnd = acfLine.indexOf('\"', nameStart + 1);
                                        if (nameStart != -1 && nameEnd != -1) {
                                            gameName = acfLine.mid(nameStart + 1, nameEnd - nameStart - 1);
                                            break;
                                        }
                                    }
                                }
                                
                                acfFile.close();
                                
                                if (!gameName.isEmpty()) {
                                    games[appId] = gameName;
                                    qCDebug(steam) << "Found game:" << gameName << "(App ID:" << appId << ")";
                                } else {
                                    qCDebug(steam) << "Could not find name for App ID" << appId;
                                }
                            } else {
                                qCDebug(steam) << "Could not open appmanifest for App ID" << appId << "at" << acfPath;
                            }
                        }
                    }
                }
            }
        }

        file.close();
        qCDebug(steam) << "Total games found:" << games.size();
        return games;
    }

    /**
     * @brief Creates button entities for all discovered games
     * @param games Map of App ID to game name
     * 
     * @details
     * Creates a button entity for each game that can be pressed
     * from Home Assistant to launch the game.
     */
    void createGameEntities(const QMap<QString, QString> &games)
    {
        m_gameList = games;
        const auto cfg = KSharedConfig::openConfig();
        KConfigGroup grp = cfg->group("steam");
        QStringList options;
        options.append("Default");
        for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
            const QString &gameName = it.value();
            if (!grp.readEntry(sanitizeGameName(gameName), false)) continue;
            options.append(gameName);

        }
        m_select->setOptions(options);
        m_select->setState("Default");
    }

    /**
     * @brief Recursively searches for a file
     * @param dir Directory to search in
     * @param depth Current recursion depth
     * @param maxDepth Maximum recursion depth
     * @return Path to found file, or empty string
     */
    QString recursiveFind(const QDir &dir, int depth, int maxDepth)
    {
        if (depth > maxDepth) return QString();

        QFileInfoList children = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : children) {
            if (fi.isFile() && fi.fileName() == "libraryfolders.vdf") {
                QString filePath = fi.absoluteFilePath();
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    QString firstLine = in.readLine().trimmed();
                    file.close();
                    if (firstLine.contains("libraryfolders")) {
                        return filePath;
                    }
                }
            } else if (fi.isDir()) {
                QString dirName = fi.fileName();
                if (dirName.startsWith(".") || 
                    dirName == "proc" || 
                    dirName == "sys" || 
                    dirName == "dev" ||
                    dirName.contains("wine") ||
                    dirName.contains("proton") ||
                    dirName.contains("dosdevices")) {
                    continue;
                }
                
                QString result = recursiveFind(QDir(fi.absoluteFilePath()), depth + 1, maxDepth);
                if (!result.isEmpty()) return result;
            }
        }
        return QString();
    }

private:
    QMap<QString, QString> m_gameList; /**< @brief Map of App ID to button entities */
    Select *m_select;
};


class Heroic : public QObject
{
    Q_OBJECT

public:
    explicit Heroic(QObject *parent = nullptr)
        : QObject(parent)
    {
        // Find all Heroic game stores
        QMap<QString, GameData> games = getAllHeroicGames();
        if (games.isEmpty()) {
            qCWarning(heroic) << "No Heroic games found. Heroic integration disabled.";
            return;
        }
        
        ensureConfig(games);
        
        qCDebug(heroic) << "Found" << games.size() << "Heroic games";
        m_select = new Select(this);
        m_select->setId("heroic_launcher");
        m_select->setName("Heroic Launcher");
        m_select->setDiscoveryConfig("icon","mdi:gamepad-square")
        connect(m_select, &Select::optionSelected, this, &Heroic::onOptionSelected);
        createGameEntities(games);
    }

private slots:
    /**
     * @brief Slot called when an option is selected
     * @param option The Heroic game name to launch
     * 
     * @details
     * Launches the specified game using Heroic's URI scheme.
     */
    void onOptionSelected(const QString &option)
    {
        if (option == "Default") return;
        
        if (!m_gameData.contains(option)) {
            qCWarning(heroic) << "Game not found in data:" << option;
            return;
        }
        
        GameData data = m_gameData[option];
        
        qCDebug(heroic) << "Launching Heroic game:" << option 
                       << "(appName:" << data.appName << ", runner:" << data.runner << ")";
        
        QString launchCommand = QString("xdg-open heroic://launch?appName=%1&runner=%2")
                               .arg(data.appName)
                               .arg(data.runner);
        QStringList args = QProcess::splitCommand(launchCommand);  
           
        QString program = args.takeFirst();           
        QProcess *process = new QProcess(this);
        
        process->setProgram(program);
        process->setArguments(args);
        if (KSandbox::isFlatpak()) {
            KSandbox::ProcessContext ctx = KSandbox::makeHostContext(*process);
            process->setProgram(ctx.program);
            process->setArguments(ctx.arguments);
        }
        // TODO, should probably make this detached instead
        process->start();      

  
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, option, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                        qCDebug(heroic) << "Successfully launched game:" << option;
                    } else {
                        qCWarning(heroic) << "Failed to launch game" << option 
                                        << ": exit code" << exitCode;
                    }
                    process->deleteLater();
                });
    }

private:
    struct GameData {
        QString appName;
        QString runner;
        QString title;
    };

    /**
     * @brief Sanitizes a game name for use as a config key
     * @param gameName The original game name
     * @return Sanitized string safe for config keys
     */
    QString sanitizeGameName(const QString &gameName)
    {
        QString id = gameName.toLower();
        id.replace(invalidCharRegex, QStringLiteral("_"));
        if (!id.isEmpty() && id[0].isDigit()) {
            id.prepend("game_");
        }
        return id;
    }

    /**
     * @brief Ensures configuration has entries for all discovered games
     * @param games Map of game title to GameData
     */
    void ensureConfig(const QMap<QString, GameData> &games)
    {
        auto cfg = KSharedConfig::openConfig();
        KConfigGroup grp = cfg->group("heroic");
    
        bool configChanged = false;
        
        // For each discovered game
        for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
            const QString &gameTitle = it.key();
            QString configKey = sanitizeGameName(gameTitle);
        
            // Check if this game already has a config entry
            if (!grp.hasKey(configKey)) {
                // New game - add to config with default expose=false
                grp.writeEntry(configKey, false);
                configChanged = true;
                qCDebug(heroic) << "Added new heroic game to config:" << configKey << "= false";
            }
        }
    
        // Get all current config keys
        const QStringList currentKeys = grp.keyList();
    
        // Remove games from config that are no longer installed
        for (const QString &configKey : currentKeys) {
            bool gameStillExists = false;
        
            // Check if this config key corresponds to any current game
            for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
                const QString &gameTitle = it.key();
                if (sanitizeGameName(gameTitle) == configKey) {
                    gameStillExists = true;
                    break;
                }
            }
        
            if (!gameStillExists) {
                grp.deleteEntry(configKey);
                configChanged = true;
                qCDebug(heroic) << "Removed unavailable game from config:" << configKey;
            }
        }
    
        if (configChanged) {
            cfg->sync();
            qCDebug(heroic) << "Heroic configuration updated with current games";
        }
    }

    /**
     * @brief Gets all installed games from all Heroic stores
     * @return Map of game title to GameData
     */
    QMap<QString, GameData> getAllHeroicGames()
    {
        QMap<QString, GameData> games;
        
        // Epic Games Store (Legendary)
        QString epicPath = QDir::homePath() + "/.config/heroic/legendaryConfig/legendary/installed.json";
        games.insert(getEpicGames(epicPath));
        
        // GOG Store
        QString gogPath = QDir::homePath() + "/.config/heroic/gog_store/installed.json";
        games.insert(getGogGames(gogPath));
        
        // Prime Gaming (Nile)
        QString primePath = QDir::homePath() + "/.config/heroic/nile_config/nile/installed.json";
        games.insert(getPrimeGames(primePath));
        
        // Sideloaded apps
        QString sideloadPath = QDir::homePath() + "/.config/heroic/sideload_apps/library.json";
        games.insert(getSideloadGames(sideloadPath));
        
        return games;
    }

    /**
     * @brief Gets Epic Games Store games
     * @param filePath Path to installed.json
     * @return Map of game title to GameData
     */
    QMap<QString, GameData> getEpicGames(const QString &filePath)
    {
        QMap<QString, GameData> games;
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCDebug(heroic) << "Could not open Epic games file:" << filePath;
            return games;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            qCWarning(heroic) << "Invalid JSON in Epic games file";
            return games;
        }
        
        QJsonObject root = doc.object();
        
        for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
            QString appName = it.key();
            QJsonObject gameObj = it.value().toObject();
            
            // Skip DLCs
            if (gameObj["is_dlc"].toBool()) {
                continue;
            }
            
            QString title = gameObj["title"].toString();
            if (!title.isEmpty()) {
                GameData data;
                data.appName = appName;
                data.runner = "legendary";
                data.title = title;
                
                games[title] = data;
                qCDebug(heroic) << "Found Epic game:" << title << "(appName:" << appName << ")";
            }
        }
        
        return games;
    }

    /**
     * @brief Gets GOG games
     * @param filePath Path to installed.json
     * @return Map of game title to GameData
     */
    QMap<QString, GameData> getGogGames(const QString &filePath)
    {
        QMap<QString, GameData> games;
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCDebug(heroic) << "Could not open GOG games file:" << filePath;
            return games;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            qCWarning(heroic) << "Invalid JSON in GOG games file";
            return games;
        }
        
        QJsonObject root = doc.object();
        QJsonArray installed = root["installed"].toArray();
        
        for (const QJsonValue &value : installed) {
            QJsonObject gameObj = value.toObject();
            
            // Skip DLCs
            if (gameObj["is_dlc"].toBool()) {
                continue;
            }
            
            QString appName = gameObj["appName"].toString();
            // GOG JSON doesn't have title, we need to get it from somewhere else
            // For now, use appName as title
            QString title = appName;
            
            GameData data;
            data.appName = appName;
            data.runner = "gog";
            data.title = title;
            
            games[title] = data;
            qCDebug(heroic) << "Found GOG game:" << title << "(appName:" << appName << ")";
        }
        
        return games;
    }

    /**
     * @brief Gets Prime Gaming games
     * @param filePath Path to installed.json
     * @return Map of game title to GameData
     */
    QMap<QString, GameData> getPrimeGames(const QString &filePath)
    {
        // TODO parse game name from library.json as its missing in installed.json
        QMap<QString, GameData> games;
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCDebug(heroic) << "Could not open Prime games file:" << filePath;
            return games;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isArray()) {
            qCWarning(heroic) << "Invalid JSON in Prime games file";
            return games;
        }
        
        QJsonArray root = doc.array();
        
        for (const QJsonValue &value : root) {
            QJsonObject gameObj = value.toObject();
            
            QString appName = gameObj["id"].toString();
            // Prime JSON doesn't have title either
            QString title = appName;
            
            GameData data;
            data.appName = appName;
            data.runner = "nile";
            data.title = title;
            
            games[title] = data;
            qCDebug(heroic) << "Found Prime game:" << title << "(appName:" << appName << ")";
        }
        
        return games;
    }

    /**
     * @brief Gets sideloaded games
     * @param filePath Path to library.json
     * @return Map of game title to GameData
     */
    QMap<QString, GameData> getSideloadGames(const QString &filePath)
    {
        QMap<QString, GameData> games;
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCDebug(heroic) << "Could not open sideload games file:" << filePath;
            return games;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            qCWarning(heroic) << "Invalid JSON in sideload games file";
            return games;
        }
        
        QJsonObject root = doc.object();
        QJsonArray gamesArray = root["games"].toArray();
        
        for (const QJsonValue &value : gamesArray) {
            QJsonObject gameObj = value.toObject();
            
            // Skip if not installed
            if (!gameObj["is_installed"].toBool()) {
                continue;
            }
            
            // Skip DLCs
            QJsonObject installObj = gameObj["install"].toObject();
            if (installObj["is_dlc"].toBool()) {
                continue;
            }
            
            QString appName = gameObj["app_name"].toString();
            QString title = gameObj["title"].toString();
            QString runner = gameObj["runner"].toString();
            
            if (!title.isEmpty()) {
                GameData data;
                data.appName = appName;
                data.runner = runner;
                data.title = title;
                
                games[title] = data;
                qCDebug(heroic) << "Found sideload game:" << title 
                               << "(appName:" << appName << ", runner:" << runner << ")";
            }
        }
        
        return games;
    }

    /**
     * @brief Creates select entities for all discovered games
     * @param games Map of game title to GameData
     */
    void createGameEntities(const QMap<QString, GameData> &games)
    {
        m_gameData = games;
        const auto cfg = KSharedConfig::openConfig();
        KConfigGroup grp = cfg->group("heroic");
        QStringList options;
        options.append("Default");
        
        for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
            const QString &gameTitle = it.key();
            if (!grp.readEntry(sanitizeGameName(gameTitle), false)) continue;
            options.append(gameTitle);
        }
        
        m_select->setOptions(options);
        m_select->setState("Default");
    }

private:
    QMap<QString, GameData> m_gameData; /**< @brief Map of game title to GameData */
    Select *m_select;
};

/**
 * @brief Checks if Steam is installed on the system
 * @return True if Steam is found, false otherwise
 * 
 * @details
 * Checks for Steam installation by looking for:
 * 1. Steam executable in PATH
 * 2. Steam desktop file
 * 3. Steam installation directory
 */
bool isSteamInstalled()
{
    // Check if steam command is in PATH
    QProcess process;
    process.start("which", QStringList() << "steam");
    process.waitForFinished();
    if (process.exitCode() == 0) {
        return true;
    }

    // Check for Steam desktop file
    QStringList desktopPaths = {
        QDir::homePath() + "/.local/share/applications/steam.desktop",
        "/usr/share/applications/steam.desktop",
        "/var/lib/flatpak/exports/share/applications/com.valvesoftware.Steam.desktop",
    };

    for (const QString &desktopPath : desktopPaths) {
        if (QFile::exists(desktopPath)) {
            return true;
        }
    }

    // Check for Steam installation directory
    QString steamHome = QDir::homePath() + "/.local/share/Steam";
    if (QDir(steamHome).exists()) {
        return true;
    }
    return false;
}

/**
 * @brief Checks if Heroic is installed on the system
 * @return True if Heroic is found, false otherwise
 * 
 * @details
 * Checks for Heroic installation by looking for:
 * 1. Heroic executable in PATH
 * 2. Heroic desktop file
 * 3. Heroic installation directory
 */
bool isHeroicInstalled()
{
    // Check if steam command is in PATH
    QProcess process;
    process.start("which", QStringList() << "heroic");
    process.waitForFinished();
    if (process.exitCode() == 0) {
        return true;
    }

    // Check for Steam desktop file
    QStringList desktopPaths = {
        QDir::homePath() + "/.local/share/applications/heroic.desktop",
    };

    for (const QString &desktopPath : desktopPaths) {
        if (QFile::exists(desktopPath)) {
            return true;
        }
    }

    // Check for Steam installation directory
    QString heroicHome = QDir::homePath() + "/.config/heroic/";
    if (QDir(heroicHome).exists()) {
        return true;
    }
    return false;
}
/**
 * @brief Sets up the GameLauncher integration
 * 
 * @details
 * Factory function called by the integration framework to create and
 * initialize a GameLauncher instance.
 */
void setupGameLauncher()
{
    bool foundlauncer = false;
    if(isSteamInstalled())
    {
        qCDebug(gl) << "Found Steam";
        foundlauncer = true;
        new Steam(qApp);
    }
    if(isHeroicInstalled())
    {
        qCDebug(gl) << "Found Heroic";
        foundlauncer = true;
        new Heroic(qApp);
    }

    if(!foundlauncer)
    {
        qCDebug(gl) << "No Game Launcher found";
    }
}
REGISTER_INTEGRATION("GameLauncher", setupGameLauncher, true)

#include "gamelauncher.moc"
