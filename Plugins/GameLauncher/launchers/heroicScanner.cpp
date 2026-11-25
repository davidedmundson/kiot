// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "heroicScanner.h"
#include <qcoreapplication.h>
#include <qloggingcategory.h>
#include <qobject.h>
#include "gamebase.h"
DEFINE_PLUGIN_LOGGER(heroiclogger, GameLauncher.HeroicScanner)
/**
 * @class HeroicScanner
 * @brief Heroic Game Scanner
 *
 * @details
 * TODO Add support for zoom games too
 * This discovers installed games from Heroic
 * and creates a sinheroicloggere select entity for launching them directly from Home Assistant.
 * All games are grouped by launcher and sorted alphabetically.
 */
HeroicScanner::HeroicScanner(QObject *parent) : GameBase(parent)
{

    
}

bool HeroicScanner::isLauncherInstalled() const
{

    // Check if heroic command is in PATH (with Flatpak escape)
    QString launchCommand = "which heroic";
    QStringList args = QProcess::splitCommand(launchCommand);
    if (!args.isEmpty()) {
        QString program = args.takeFirst();
        QProcess process;
        process.setProgram(program);
        process.setArguments(args);
        
        if (PlatformHelper::isFlatpak()) {
            auto ctx = PlatformHelper::makeHostContext(process);
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


QMap<QString, GameData>  HeroicScanner::scanGames()
{
    if (!isLauncherInstalled()) {
        qCDebug(heroiclogger) << "Heroic is not installed.";
        return QMap<QString, GameData>();
    }
    else{

    
    qCDebug(heroiclogger) << "Discovering Heroic games...";
    QMap<QString, GameData> games;
        
    QString heroic_data_path;

    QString nativePath = QDir::homePath() + "/.config/heroic/";
    QString flatpakPath = QDir::homePath() + "/.var/app/com.heroicgameslauncher.hgl/config/heroic/";

    if (QDir(nativePath).exists()) {
        heroic_data_path = "/.config/heroic/";
    } else if (QDir(flatpakPath).exists()) {
        heroic_data_path = "/.var/app/com.heroicgameslauncher.hgl/config/heroic/";
    } else {
        // ingen Heroic installasjon funnet, returner tidlig
        return QMap<QString, GameData>();
    }
    m_configPath = QDir::homePath() + heroic_data_path;
    // Epic Games Store (Legendary)
    QString epicPath = QDir::homePath() + heroic_data_path + "legendaryConfig/legendary/installed.json";
    games.insert(getEpicGames(epicPath));

    // GOG Store
    QString gogPath = QDir::homePath() + heroic_data_path + "gog_store/installed.json";
    games.insert(getGogGames(gogPath));

    // Prime Gaming (Nile)
    QString primePath = QDir::homePath() + heroic_data_path + "nile_config/nile/installed.json";
    games.insert(getPrimeGames(primePath));

    // Sideloaded apps
    QString sideloadPath = QDir::homePath() + heroic_data_path + "sideload_apps/library.json";
    games.insert(getSideloadGames(sideloadPath));
    m_games = games;
    }
    return m_games;
    
}


QMap<QString, GameData> HeroicScanner::getHeroicGames()
{
    QMap<QString, GameData> games;
        
    QString heroic_data_path;

    QString nativePath = QDir::homePath() + "/.config/heroic/";
    QString flatpakPath = QDir::homePath() + "/.var/app/com.heroicgameslauncher.hgl/config/heroic/";

    if (QDir(nativePath).exists()) {
        heroic_data_path = "/.config/heroic/";
    } else if (QDir(flatpakPath).exists()) {
        heroic_data_path = "/.var/app/com.heroicgameslauncher.hgl/config/heroic/";
    } else {
        // ingen Heroic installasjon funnet, returner tidlig
        return QMap<QString, GameData>();
    }
    // Epic Games Store (Legendary)
    QString epicPath = QDir::homePath() + heroic_data_path + "legendaryConfig/legendary/installed.json";
    games.insert(getEpicGames(epicPath));

    // GOG Store
    QString gogPath = QDir::homePath() + heroic_data_path + "gog_store/installed.json";
    games.insert(getGogGames(gogPath));

    // Prime Gaming (Nile)
    QString primePath = QDir::homePath() + heroic_data_path + "nile_config/nile/installed.json";
    games.insert(getPrimeGames(primePath));

    // Sideloaded apps
    QString sideloadPath = QDir::homePath() + heroic_data_path + "sideload_apps/library.json";
    games.insert(getSideloadGames(sideloadPath));

    return games;
}

QMap<QString, GameData> HeroicScanner::getEpicGames(const QString &filePath)
{
    QMap<QString, GameData> games;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCDebug(heroiclogger) << "Could not open Epic games file:" << filePath;
        return games;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qCWarning(heroiclogger) << "Invalid JSON in Epic games file";
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
        QString gameId = gameObj["app_name"].toString();
        QString winePrefix = readGameCOnfig(gameId);
        QString installPath = gameObj["install_path"].toString();
        QString executable = gameObj["executable"].toString();
        QString fullPath  = QDir::cleanPath(installPath + "/" + executable);
        QFileInfo fileInfo(fullPath);
        if (fileInfo.exists() && fileInfo.isFile()) {
            executable = fullPath;
        } 
        if (!title.isEmpty()) {
            GameData data;
            data.gameId = gameId;
            data.gameName = title;
            data.launcher = launcherName();
            data.runner = "legendary";
            data.installPath = installPath;
            data.prefixPath = winePrefix;
            data.exePath = executable;
            games[title] = data;
        }
    }

    return games;
}
QString HeroicScanner::readGameCOnfig(const QString &appName)
{
    QString GameConfigPath = m_configPath + "/GamesConfig/" + appName + ".json";
    QFile conffile(GameConfigPath);
    if (!conffile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCDebug(heroiclogger) << "Could not open gog GameConfig file for game" << appName << ":" << GameConfigPath;
        return "UNKNOWN";
    }

    QByteArray data = conffile.readAll();
    conffile.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qCWarning(heroiclogger) << "Invalid JSON in GameConfig file for:" << appName ;
        return "";
    }
    QJsonObject root = doc.object();
    QJsonObject installed = root[appName].toObject();
    return installed.value("winePrefix").toString("UNKNOWN");
}
QMap<QString, GameData> HeroicScanner::getGogGames(const QString &filePath)
{
    QMap<QString, GameData> games;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCDebug(heroiclogger) << "Could not open GOG games file:" << filePath;
        return games;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qCWarning(heroiclogger) << "Invalid JSON in GOG games file";
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
        QString prefixpath = readGameCOnfig(appName);
        GameData data;
        data.gameId = appName;
        data.launcher = launcherName();
        data.gameName = title;
        data.runner = "gog";
        data.installPath = gameObj["install_path"].toString();
        data.prefixPath = prefixpath;
        games[title] = data;
    }

    return games;
}

QMap<QString, GameData> HeroicScanner::getPrimeGames(const QString &filePath)
{
    QMap<QString, GameData> games;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCDebug(heroiclogger) << "Could not open Prime games file:" << filePath;
        return games;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray()) {
        qCWarning(heroiclogger) << "Invalid JSON in Prime games file";
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
        data.launcher = launcherName();
        data.runner = "nile";
        games[title] = data;
    }

    return games;
}

QMap<QString, GameData> HeroicScanner::getSideloadGames(const QString &filePath)
{
    QMap<QString, GameData> games;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCDebug(heroiclogger) << "Could not open sideload games file:" << filePath;
        return games;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qCWarning(heroiclogger) << "Invalid JSON in sideload games file";
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
        QString prefixpath = readGameCOnfig(appName);
        QString title = gameObj["title"].toString();
        QString runner = gameObj["runner"].toString();
        QString exepath = installObj["executable"].toString();

        if (!title.isEmpty()) {
            GameData data;
            data.gameId = appName;
            data.launcher = launcherName();
            data.gameName = title;
            data.runner = runner;
            data.prefixPath = prefixpath;
            data.exePath = exepath;
            data.installPath = gameObj["folder_name"].toString();
            games[title] = data;
        }
    }

    return games;
}
