// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/select.h"

#include <KConfigGroup>
#include <KSandbox>
#include <KSharedConfig>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QTextStream>
#include <QVariantMap>
#include <QTimer>
#include <QApplication>
#include <QByteArray>
#include <QLoggingCategory>
#include <QProcess>
#include <QCollator>
#include <QLocale>
#include <algorithm>

Q_DECLARE_LOGGING_CATEGORY(gl)
Q_LOGGING_CATEGORY(gl, "integration.GameLauncher")

#include <QRegularExpression>
namespace
{
static const QRegularExpression invalidCharRegex("[^a-zA-Z0-9_-]");
}

/**
 * @class GameLauncher
 * @brief Unified game launcher integration for Kiot
 *
 * @details
 * This integration discovers installed games from Steam, Heroic, and Lutris
 * and creates a single select entity for launching them directly from Home Assistant.
 * All games are grouped by launcher and sorted alphabetically.
 */
class GameLauncher : public QObject
{
    Q_OBJECT

public:
    explicit GameLauncher(QObject *parent = nullptr)
        : QObject(parent)
        , m_select(nullptr)
    {
        // Discover games from all launchers
        discoverAllGames();
        
        if (m_games.isEmpty()) {
            qCWarning(gl) << "No games found from any launcher. GameLauncher integration disabled.";
            return;
        }

        ensureConfig();
        createGameEntity();
    }

private slots:
    /**
     * @brief Slot called when an option is selected
     * @param option The game identifier in format "Launcher - GameName"
     *
     * @details
     * Launches the specified game using the appropriate launcher's URI scheme.
     */
    void onOptionSelected(const QString &option)
    {
        if (option == "Default" || !m_select) {
            return;
        }

        if (!m_games.contains(option)) {
            qCWarning(gl) << "Game not found in data:" << option;
            setToDefault();
            return;
        }

        GameData data = m_games[option];
        qCDebug(gl) << "Launching game:" << data.displayName << "(Launcher:" << data.launcher << ")";

        QString launchCommand;
        if (data.launcher == "Steam") {
            launchCommand = QString("xdg-open steam://rungameid/%1").arg(data.gameId);
        } else if (data.launcher == "Heroic") {
            launchCommand = QString("xdg-open heroic://launch?appName=%1&runner=%2").arg(data.gameId).arg(data.runner);
        } else if (data.launcher == "Lutris") {
            launchCommand = QString("env LUTRIS_SKIP_INIT=1 lutris lutris:rungameid/%1").arg(data.gameId);
        } else {
            qCWarning(gl) << "Unknown launcher:" << data.launcher;
            setToDefault();
            return;
        }

        QStringList args = QProcess::splitCommand(launchCommand);
        if (args.isEmpty()) {
            qCWarning(gl) << "Could not parse launch command:" << launchCommand;
            setToDefault();
            return;
        }

        QString program = args.takeFirst();

        if (KSandbox::isFlatpak()) {
            QProcess tempProcess;
            tempProcess.setProgram(program);
            tempProcess.setArguments(args);
    
            KSandbox::ProcessContext ctx = KSandbox::makeHostContext(tempProcess);
    
            bool success = QProcess::startDetached(ctx.program, ctx.arguments);
    
            if (success) {
                qCDebug(gl) << "Successfully launched game (detached):" << option;
    }        else {
                qCWarning(gl) << "Failed to launch game (detached):" << option;
            }
        } else {
            bool success = QProcess::startDetached(program, args);
    
            if (success) {
                qCDebug(gl) << "Successfully launched game (detached):" << option;
            } else {
                qCWarning(gl) << "Failed to launch game (detached):" << option;
            }
        }

        setToDefault();
    }

private:
    struct GameData {
        QString launcher;      // "Steam", "Heroic", or "Lutris"
        QString gameId;        // App ID for Steam, appName for Heroic, game ID for Lutris
        QString gameName;      // Original game name
        QString displayName;   // "Launcher - GameName"
        QString runner;        // For Heroic: "legendary", "gog", "nile", or runner name
    };
    void setToDefault()
    {
        if (m_select) {
            QTimer::singleShot(100, this, [this]() {
                m_select->setState("Default");
            });
        }
    }
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
     * @brief Sorts a list of strings alphabetically
     */
    QList<QString> sortAlphabetically(const QList<QString> &input)
    {
        QList<QString> sorted = input;

        QCollator collator(QLocale::system());
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);

        std::sort(sorted.begin(), sorted.end(),
                  [&collator](const QString &a, const QString &b) {
                      return collator.compare(a, b) < 0;
                  });

        return sorted;
    }

    /**
     * @brief Discovers games from all available launchers
     */
    void discoverAllGames()
    {
        // Check and discover Steam games
        if (isSteamInstalled()) {
            qCDebug(gl) << "Discovering Steam games...";
            QMap<QString, QString> steamGames = getSteamGames();
            for (auto it = steamGames.constBegin(); it != steamGames.constEnd(); ++it) {
                GameData data;
                data.launcher = "Steam";
                data.gameId = it.key();
                data.gameName = it.value();
                data.displayName = QString("Steam - %1").arg(data.gameName);
                data.runner = "";
                
                m_games[data.displayName] = data;
                qCDebug(gl) << "Found Steam game:" << data.gameName << "(App ID:" << data.gameId << ")";
            }
        }

        // Check and discover Heroic games
        if (isHeroicInstalled()) {
            qCDebug(gl) << "Discovering Heroic games...";
            QMap<QString, GameData> heroicGames = getHeroicGames();
            for (auto it = heroicGames.constBegin(); it != heroicGames.constEnd(); ++it) {
                GameData data = it.value();
                data.launcher = "Heroic";
                data.displayName = QString("Heroic - %1").arg(data.gameName);
                
                m_games[data.displayName] = data;
                qCDebug(gl) << "Found Heroic game:" << data.gameName << "(runner:" << data.runner << ")";
            }
        }

        // Check and discover Lutris games
        if (isLutrisInstalled()) {
            qCDebug(gl) << "Discovering Lutris games...";
            QMap<QString, QString> lutrisGames = getLutrisGames();
            for (auto it = lutrisGames.constBegin(); it != lutrisGames.constEnd(); ++it) {
                GameData data;
                data.launcher = "Lutris";
                data.gameId = it.value();
                data.gameName = it.key();
                data.displayName = QString("Lutris - %1").arg(data.gameName);
                data.runner = "";
                
                m_games[data.displayName] = data;
                qCDebug(gl) << "Found Lutris game:" << data.gameName << "(game ID:" << data.gameId << ")";
            }
        }

        qCInfo(gl) << "Total games discovered:" << m_games.size();
    }

    /**
     * @brief Ensures configuration has entries for all discovered games
     */
    void ensureConfig()
    {
        auto cfg = KSharedConfig::openConfig();
        KConfigGroup grp = cfg->group("gamelauncher");

        bool configChanged = false;

        // For each discovered game
        for (auto it = m_games.constBegin(); it != m_games.constEnd(); ++it) {
            const QString &displayName = it.key();
            QString configKey = sanitizeGameName(displayName);

            // Check if this game already has a config entry
            if (!grp.hasKey(configKey)) {
                // New game - add to config with default expose=true
                grp.writeEntry(configKey, true);
                configChanged = true;
                qCDebug(gl) << "Added new game to config:" << configKey << "= false";
            }
        }

        // Get all current config keys
        const QStringList currentKeys = grp.keyList();

        // Remove games from config that are no longer installed
        for (const QString &configKey : currentKeys) {
            bool gameStillExists = false;

            // Check if this config key corresponds to any current game
            for (auto it = m_games.constBegin(); it != m_games.constEnd(); ++it) {
                const QString &displayName = it.key();
                if (sanitizeGameName(displayName) == configKey) {
                    gameStillExists = true;
                    break;
                }
            }

            if (!gameStillExists) {
                grp.deleteEntry(configKey);
                configChanged = true;
                qCDebug(gl) << "Removed unavailable game from config:" << configKey;
            }
        }

        if (configChanged) {
            cfg->sync();
            qCDebug(gl) << "GameLauncher configuration updated with current games";
        }
    }

    /**
     * @brief Creates a single select entity for all discovered games
     */
    void createGameEntity()
    {
        m_select = new Select(this);
        m_select->setId("game_launcher");
        m_select->setName("Game Launcher");
        m_select->setDiscoveryConfig("icon", "mdi:gamepad-variant");
        
        QStringList options;
        

        const auto cfg = KSharedConfig::openConfig();
        KConfigGroup grp = cfg->group("gamelauncher");

        // Add games that are enabled in config
        for (auto it = m_games.constBegin(); it != m_games.constEnd(); ++it) {
            const QString &displayName = it.key();
            if (grp.readEntry(sanitizeGameName(displayName), false)) {
                options.append(displayName);
            }
        }

        // Sort alphabetically
        options = sortAlphabetically(options);
        options.prepend("Default");
        m_select->setOptions(options);
        m_select->setState("Default");
        
        connect(m_select, &Select::optionSelected, this, &GameLauncher::onOptionSelected);
        
        qCInfo(gl) << "Exposed" << options.size() << "games in select entity";
    }

    // ========== Steam Functions ==========
    
    bool isSteamInstalled()
    {
        // Check if steam command is in PATH (with Flatpak escape)
        QString launchCommand = "which steam";
        QStringList args = QProcess::splitCommand(launchCommand);
        if (!args.isEmpty()) {
            QString program = args.takeFirst();
            QProcess process;
            process.setProgram(program);
            process.setArguments(args);
        
            if (KSandbox::isFlatpak()) {
                KSandbox::ProcessContext ctx = KSandbox::makeHostContext(process);
                process.setProgram(ctx.program);
                process.setArguments(ctx.arguments);
            }
        
            process.start();
            process.waitForFinished();
        
            if (process.exitCode() == 0) {
                return true;
            }       
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

    QMap<QString, QString> getSteamGames()
    {
        QMap<QString, QString> games;
        QString libraryConfig = findSteamLibraryConfig();
        
        if (libraryConfig.isEmpty()) {
            qCDebug(gl) << "Could not find Steam library configuration";
            return games;
        }

        QFile file(libraryConfig);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(gl) << "Failed to open Steam config:" << libraryConfig;
            return games;
        }

        QTextStream in(&file);
        QString currentLibraryPath;
        bool inAppsSection = false;
        int braceDepth = 0;
        int appsBraceDepth = 0;

        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.trimmed().isEmpty())
                continue;

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

            // Look for library path
            if (line.contains("\"path\"\t\t\"")) {
                int startPos = line.indexOf("\"path\"");
                startPos = line.indexOf('\"', startPos + 6);
                if (startPos != -1) {
                    int endPos = line.indexOf('\"', startPos + 1);
                    if (endPos != -1) {
                        currentLibraryPath = line.mid(startPos + 1, endPos - startPos - 1);
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
                            QString acfPath = QDir(currentLibraryPath).filePath(QString("steamapps/appmanifest_%1.acf").arg(appId));

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
                                }
                            }
                        }
                    }
                }
            }
        }

        file.close();
        return games;
    }

    QString findSteamLibraryConfig()
    {
        QStringList standardPaths = {
            QDir::homePath() + "/.local/share/Steam/config/libraryfolders.vdf",
            QDir::homePath() + "/.steam/steam/config/libraryfolders.vdf",
            QDir::homePath() + "/.var/app/com.valvesoftware.Steam/data/Steam/config/libraryfolders.vdf",
            "/home/steam/.local/share/Steam/config/libraryfolders.vdf"
        };

        for (const QString &path : standardPaths) {
            if (QFile::exists(path)) {
                return path;
            }
        }

        QString steamHome = QDir::homePath() + "/.local/share/Steam";
        if (QDir(steamHome).exists()) {
            QString foundPath = recursiveFind(QDir(steamHome), 0, 3);
            if (!foundPath.isEmpty()) {
                return foundPath;
            }
        }

        return recursiveFind(QDir(QDir::homePath()), 0, 3);
    }

    // ========== Heroic Functions ==========
    
    bool isHeroicInstalled()
    {
        // Check if heroic command is in PATH (with Flatpak escape)
        QString launchCommand = "which heroic";
        QStringList args = QProcess::splitCommand(launchCommand);
        if (!args.isEmpty()) {
            QString program = args.takeFirst();
            QProcess process;
            process.setProgram(program);
            process.setArguments(args);
        
            if (KSandbox::isFlatpak()) {
                KSandbox::ProcessContext ctx = KSandbox::makeHostContext(process);
                process.setProgram(ctx.program);
                process.setArguments(ctx.arguments);
            }
        
            process.start();
            process.waitForFinished();
        
            if (process.exitCode() == 0) {
                return true;
            }       
        }

        // Check for Heroic desktop file
        QStringList desktopPaths = {
            QDir::homePath() + "/.local/share/applications/heroic.desktop",
            "/usr/share/applications/heroic.desktop",
            "/var/lib/flatpak/exports/share/applications/com.heroicgameslauncher.hgl.desktop",
        };

        for (const QString &desktopPath : desktopPaths) {
            if (QFile::exists(desktopPath)) {
                return true;
            }
        }

        // Check for Heroic installation directory
        QString heroicHome = QDir::homePath() + "/.config/heroic/";
        if (QDir(heroicHome).exists()) {
            return true;
        }
        return false;
    }

    QMap<QString, GameData> getHeroicGames()
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

    QMap<QString, GameData> getEpicGames(const QString &filePath)
    {
        QMap<QString, GameData> games;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCDebug(gl) << "Could not open Epic games file:" << filePath;
            return games;
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            qCWarning(gl) << "Invalid JSON in Epic games file";
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
                data.gameId = appName;
                data.gameName = title;
                data.runner = "legendary";
                games[title] = data;
            }
        }

        return games;
    }

    QMap<QString, GameData> getGogGames(const QString &filePath)
    {
        QMap<QString, GameData> games;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCDebug(gl) << "Could not open GOG games file:" << filePath;
            return games;
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            qCWarning(gl) << "Invalid JSON in GOG games file";
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
            QString title = appName; // GOG JSON doesn't have title

            GameData data;
            data.gameId = appName;
            data.gameName = title;
            data.runner = "gog";
            games[title] = data;
        }

        return games;
    }

    QMap<QString, GameData> getPrimeGames(const QString &filePath)
    {
        QMap<QString, GameData> games;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCDebug(gl) << "Could not open Prime games file:" << filePath;
            return games;
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isArray()) {
            qCWarning(gl) << "Invalid JSON in Prime games file";
            return games;
        }

        QJsonArray root = doc.array();

        for (const QJsonValue &value : root) {
            QJsonObject gameObj = value.toObject();

            QString appName = gameObj["id"].toString();
            QString title = appName; // Prime JSON doesn't have title

            GameData data;
            data.gameId = appName;
            data.gameName = title;
            data.runner = "nile";
            games[title] = data;
        }

        return games;
    }

    QMap<QString, GameData> getSideloadGames(const QString &filePath)
    {
        QMap<QString, GameData> games;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCDebug(gl) << "Could not open sideload games file:" << filePath;
            return games;
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            qCWarning(gl) << "Invalid JSON in sideload games file";
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
                data.gameId = appName;
                data.gameName = title;
                data.runner = runner;
                games[title] = data;
            }
        }

        return games;
    }

    // ========== Lutris Functions ==========
    
    bool isLutrisInstalled()
    {
        // Check if lutris command is in PATH (with Flatpak escape)
        QString launchCommand = "which lutris";
        QStringList args = QProcess::splitCommand(launchCommand);
        if (!args.isEmpty()) {
            QString program = args.takeFirst();
            QProcess process;
            process.setProgram(program);
            process.setArguments(args);
        
            if (KSandbox::isFlatpak()) {
                KSandbox::ProcessContext ctx = KSandbox::makeHostContext(process);
                process.setProgram(ctx.program);
                process.setArguments(ctx.arguments);
            }
        
            process.start();
            process.waitForFinished();
        
            if (process.exitCode() == 0) {
                return true;
            }       
        }

        // Check for Lutris desktop file
        QStringList desktopPaths = {
            QDir::homePath() + "/.local/share/applications/lutris.desktop",
            "/usr/share/applications/lutris.desktop",
            "/var/lib/flatpak/exports/share/applications/net.lutris.Lutris.desktop",
        };

        for (const QString &desktopPath : desktopPaths) {
            if (QFile::exists(desktopPath)) {
                return true;
            }
        }

        // Check for Lutris installation directory
        QString lutrisHome = QDir::homePath() + "/.local/share/lutris";
        if (QDir(lutrisHome).exists()) {
            return true;
        }
        return false;
    }

    QMap<QString, QString> getLutrisGames()
    {
        QMap<QString, QString> games;

        // Path to Lutris game-paths.json
        QString gamePathsPath = QDir::homePath() + "/.cache/lutris/game-paths.json";
        QString gamesDir = QDir::homePath() + "/.local/share/lutris/games";

        if (!QFile::exists(gamePathsPath)) {
            qCDebug(gl) << "Lutris game-paths.json not found at:" << gamePathsPath;
            return games;
        }

        // Read game-paths.json
        QFile gamePathsFile(gamePathsPath);
        if (!gamePathsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(gl) << "Could not open game-paths.json:" << gamePathsPath;
            return games;
        }

        QByteArray gamePathsData = gamePathsFile.readAll();
        gamePathsFile.close();

        QJsonDocument gamePathsDoc = QJsonDocument::fromJson(gamePathsData);
        if (gamePathsDoc.isNull() || !gamePathsDoc.isObject()) {
            qCWarning(gl) << "Invalid JSON in game-paths.json";
            return games;
        }

        QJsonObject gamePaths = gamePathsDoc.object();

        // Find all YAML files in games directory
        QDirIterator yamlIt(gamesDir, QStringList() << "*.yml", QDir::Files);
        QMap<QString, QString> yamlGames; // Map executable name to game name

        while (yamlIt.hasNext()) {
            QString yamlPath = yamlIt.next();
            QFile yamlFile(yamlPath);
            if (!yamlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }

            QByteArray yamlData = yamlFile.readAll();
            yamlFile.close();

            // Simple YAML parsing - looking for name and exe fields
            QString yamlContent = QString::fromUtf8(yamlData);
            QString gameName;
            QString exePath;

            // Parse name field
            QRegularExpression nameRegex("name:\\s*\"?([^\"\\n]+)\"?");
            QRegularExpressionMatch nameMatch = nameRegex.match(yamlContent);
            if (nameMatch.hasMatch()) {
                gameName = nameMatch.captured(1).trimmed();
            }

            // Parse exe field
            QRegularExpression exeRegex("exe:\\s*\"?([^\"\\n]+)\"?");
            QRegularExpressionMatch exeMatch = exeRegex.match(yamlContent);
            if (exeMatch.hasMatch()) {
                exePath = exeMatch.captured(1).trimmed();
            }

            if (!exePath.isEmpty()) {
                // Extract just the executable name from the path
                QString exeName = QFileInfo(exePath).fileName();
                yamlGames[exeName] = gameName.isEmpty() ? exeName : gameName;
            }
        }

        // Match game IDs from game-paths.json with YAML game names
        for (auto it = gamePaths.constBegin(); it != gamePaths.constEnd(); ++it) {
            QString gameId = it.key();
            QString exePath = it.value().toString();

            if (exePath.isEmpty()) {
                continue;
            }

            // Extract executable name from path
            QString exeName = QFileInfo(exePath).fileName();
            QString gameName;

            // Try to find game name from YAML files
            if (yamlGames.contains(exeName)) {
                gameName = yamlGames[exeName];
            } else {
                // Fallback: use executable name without extension
                gameName = QFileInfo(exeName).completeBaseName();
            }

            if (!gameName.isEmpty()) {
                games[gameName] = gameId;
            }
        }

        return games;
    }

    // ========== Helper Functions ==========
    
    QString recursiveFind(const QDir &dir, int depth, int maxDepth)
    {
        if (depth > maxDepth)
            return QString();

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
                if (dirName.startsWith(".") || dirName == "proc" || dirName == "sys" || dirName == "dev" || dirName.contains("wine")
                    || dirName.contains("proton") || dirName.contains("dosdevices")) {
                    continue;
                }

                QString result = recursiveFind(QDir(fi.absoluteFilePath()), depth + 1, maxDepth);
                if (!result.isEmpty())
                    return result;
            }
        }
        return QString();
    }

private:
    Select *m_select;
    QMap<QString, GameData> m_games; /**< @brief Map of display name to GameData */
};

/**
 * @brief Sets up the GameLauncher integration
 *
 * @details
 * Factory function called by the integration framework to create and
 * initialize a GameLauncher instance.
 */
void setupGameLauncher()
{
    new GameLauncher(qApp);
}

REGISTER_INTEGRATION("GameLauncher", setupGameLauncher, true)

#include "gamelauncher.moc"