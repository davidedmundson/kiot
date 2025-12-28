// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

// TODO look into making this as one select entity instead of one per launcher
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

#include <QApplication>
#include <QByteArray>
#include <QLoggingCategory>
#include <QProcess>

Q_DECLARE_LOGGING_CATEGORY(gl)
Q_LOGGING_CATEGORY(gl, "integration.GameLauncher")
Q_DECLARE_LOGGING_CATEGORY(steam)
Q_LOGGING_CATEGORY(steam, "integration.GameLauncher.Steam")
Q_DECLARE_LOGGING_CATEGORY(heroic)
Q_LOGGING_CATEGORY(heroic, "integration.GameLauncher.Heroic")
Q_DECLARE_LOGGING_CATEGORY(lutris)
Q_LOGGING_CATEGORY(lutris, "integration.GameLauncher.Lutris")

#include <QRegularExpression>
namespace
{
static const QRegularExpression invalidCharRegex("[^a-zA-Z0-9_-]");
}

/**
 * @class Steam
 * @brief Steam game launcher integration for Kiot
 *
 * @details
 * This integration discovers installed Steam games and creates
 * a select entity for launching them directly from Home Assistant.
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
        m_select->setDiscoveryConfig("icon", "mdi:steam");
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
        if (option == "Default")
            return;
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

        connect(process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this,
                [this, gameId, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                        qCDebug(steam) << "Successfully launched game:" << gameId;
                    } else {
                        qCWarning(steam) << "Failed to launch game" << gameId << ": exit code" << exitCode;
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
        QStringList standardPaths = {QDir::homePath() + "/.local/share/Steam/config/libraryfolders.vdf",
                                     QDir::homePath() + "/.steam/steam/config/libraryfolders.vdf",
                                     QDir::homePath() + "/.var/app/com.valvesoftware.Steam/data/Steam/config/libraryfolders.vdf",
                                     "/home/steam/.local/share/Steam/config/libraryfolders.vdf"};

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
            if (!grp.readEntry(sanitizeGameName(gameName), false))
                continue;
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
    QMap<QString, QString> m_gameList; /**< @brief Map of App ID to button entities */
    Select *m_select;
};
/**
 * @class Heroic
 * @brief Heroic game launcher integration for Kiot
 *
 * @details
 * This integration discovers installed Heroic games and creates
 * a select entity for launching them directly from Home Assistant.
 */
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
        m_select->setDiscoveryConfig("icon", "mdi:gamepad-square");
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
        if (option == "Default")
            return;

        if (!m_gameData.contains(option)) {
            qCWarning(heroic) << "Game not found in data:" << option;
            return;
        }

        GameData data = m_gameData[option];

        qCDebug(heroic) << "Launching Heroic game:" << option << "(appName:" << data.appName << ", runner:" << data.runner << ")";

        QString launchCommand = QString("xdg-open heroic://launch?appName=%1&runner=%2").arg(data.appName).arg(data.runner);
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

        connect(process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this,
                [this, option, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                        qCDebug(heroic) << "Successfully launched game:" << option;
                    } else {
                        qCWarning(heroic) << "Failed to launch game" << option << ": exit code" << exitCode;
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
                qCDebug(heroic) << "Found sideload game:" << title << "(appName:" << appName << ", runner:" << runner << ")";
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
            if (!grp.readEntry(sanitizeGameName(gameTitle), false))
                continue;
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
 * @class Lutris
 * @brief Lutris game launcher integration for Kiot
 *
 * @details
 * This integration discovers installed Lutris games and creates
 * a select entity for launching them directly from Home Assistant.
 */
class Lutris : public QObject
{
    Q_OBJECT

public:
    explicit Lutris(QObject *parent = nullptr)
        : QObject(parent)
    {
        // Find all Lutris games
        QMap<QString, QString> games = getAllLutrisGames();
        if (games.isEmpty()) {
            qCWarning(lutris) << "No Lutris games found. Lutris integration disabled.";
            return;
        }

        ensureConfig(games);

        qCDebug(lutris) << "Found" << games.size() << "Lutris games";
        m_select = new Select(this);
        m_select->setId("lutris_launcher");
        m_select->setName("Lutris Launcher");
        m_select->setDiscoveryConfig("icon", "mdi:gamepad-variant");
        connect(m_select, &Select::optionSelected, this, &Lutris::onOptionSelected);
        createGameEntities(games);
    }

private slots:
    /**
     * @brief Slot called when an option is selected
     * @param option The Lutris game name to launch
     *
     * @details
     * Launches the specified game using Lutris's URI scheme.
     */
    void onOptionSelected(const QString &option)
    {
        if (option == "Default")
            return;

        if (!m_gameList.contains(option)) {
            qCWarning(lutris) << "Game not found in data:" << option;
            return;
        }

        QString gameId = m_gameList[option];
        qCDebug(lutris) << "Launching Lutris game:" << option << "(game ID:" << gameId << ")";

        QString launchCommand = QString("env LUTRIS_SKIP_INIT=1 lutris lutris:rungameid/%1").arg(gameId);
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

        connect(process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this,
                [this, option, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                        qCDebug(lutris) << "Successfully launched game:" << option;
                    } else {
                        qCWarning(lutris) << "Failed to launch game" << option << ": exit code" << exitCode;
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
     * @param games Map of game name to game ID
     */
    void ensureConfig(const QMap<QString, QString> &games)
    {
        auto cfg = KSharedConfig::openConfig();
        KConfigGroup grp = cfg->group("lutris");

        bool configChanged = false;

        // For each discovered game
        for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
            const QString &gameName = it.key();
            QString configKey = sanitizeGameName(gameName);

            // Check if this game already has a config entry
            if (!grp.hasKey(configKey)) {
                // New game - add to config with default expose=false
                grp.writeEntry(configKey, false);
                configChanged = true;
                qCDebug(lutris) << "Added new lutris game to config:" << configKey << "= false";
            }
        }

        // Get all current config keys
        const QStringList currentKeys = grp.keyList();

        // Remove games from config that are no longer installed
        for (const QString &configKey : currentKeys) {
            bool gameStillExists = false;

            // Check if this config key corresponds to any current game
            for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
                const QString &gameName = it.key();
                if (sanitizeGameName(gameName) == configKey) {
                    gameStillExists = true;
                    break;
                }
            }

            if (!gameStillExists) {
                grp.deleteEntry(configKey);
                configChanged = true;
                qCDebug(lutris) << "Removed unavailable game from config:" << configKey;
            }
        }

        if (configChanged) {
            cfg->sync();
            qCDebug(lutris) << "Lutris configuration updated with current games";
        }
    }

    /**
     * @brief Gets all installed games from Lutris
     * @return Map of game name to game ID
     */
    QMap<QString, QString> getAllLutrisGames()
    {
        QMap<QString, QString> games;

        // Path to Lutris game-paths.json
        QString gamePathsPath = QDir::homePath() + "/.cache/lutris/game-paths.json";
        QString gamesDir = QDir::homePath() + "/.local/share/lutris/games";

        if (!QFile::exists(gamePathsPath)) {
            qCDebug(lutris) << "Lutris game-paths.json not found at:" << gamePathsPath;
            return games;
        }

        // Read game-paths.json
        QFile gamePathsFile(gamePathsPath);
        if (!gamePathsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(lutris) << "Could not open game-paths.json:" << gamePathsPath;
            return games;
        }

        QByteArray gamePathsData = gamePathsFile.readAll();
        gamePathsFile.close();

        QJsonDocument gamePathsDoc = QJsonDocument::fromJson(gamePathsData);
        if (gamePathsDoc.isNull() || !gamePathsDoc.isObject()) {
            qCWarning(lutris) << "Invalid JSON in game-paths.json";
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
                qCDebug(lutris) << "Found Lutris game:" << gameName << "(game ID:" << gameId << ")";
            }
        }

        return games;
    }

    /**
     * @brief Creates select entities for all discovered games
     * @param games Map of game name to game ID
     */
    void createGameEntities(const QMap<QString, QString> &games)
    {
        m_gameList = games;
        const auto cfg = KSharedConfig::openConfig();
        KConfigGroup grp = cfg->group("lutris");
        QStringList options;
        options.append("Default");

        for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
            const QString &gameName = it.key();
            if (!grp.readEntry(sanitizeGameName(gameName), false))
                continue;
            options.append(gameName);
        }

        m_select->setOptions(options);
        m_select->setState("Default");
    }

private:
    QMap<QString, QString> m_gameList; /**< @brief Map of game name to game ID */
    Select *m_select;
};


/**
 * @brief Checks if Lutris is installed on the system
 * @return True if Lutris is found, false otherwise
 *
 * @details
 * Checks for Lutris installation by looking for:
 * 1. Lutris executable in PATH
 * 2. Lutris desktop file
 * 3. Lutris installation directory
 */
bool isLutrisInstalled()
{
    // Check if lutris command is in PATH
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
    // Check if heroic command is in PATH
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
    if (isSteamInstalled()) {
        qCDebug(gl) << "Found Steam";
        foundlauncer = true;
        new Steam(qApp);
    }
    if (isHeroicInstalled()) {
        qCDebug(gl) << "Found Heroic";
        foundlauncer = true;
        new Heroic(qApp);
    }
    if (isLutrisInstalled()) {
        qCDebug(gl) << "Found Lutris";
        foundlauncer = true;
        new Lutris(qApp);
    }
    if (!foundlauncer) {
        qCDebug(gl) << "No Game Launcher found";
    }
}
REGISTER_INTEGRATION("GameLauncher", setupGameLauncher, true)

#include "gamelauncher.moc"
