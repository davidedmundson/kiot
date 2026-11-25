// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "steamScanner.h"
#include <qloggingcategory.h>
#include "gamebase.h"
DEFINE_PLUGIN_LOGGER(steamlogger, GameLauncher.SteamScanner)
/**
 * @class SteamScanner
 * @brief Steam Game Scanner
 *
 * @details
 * This discovers installed games from Steam
 * and creates a single select entity for launching them directly from Home Assistant.
 * All games are grouped by launcher and sorted alphabetically.
 */
SteamScanner::SteamScanner(QObject *parent) : GameBase(parent)
{

    
}
QMap<QString, GameData>  SteamScanner::scanGames()
{
    if (!isLauncherInstalled()) {
        qCDebug(steamlogger) << "Steam is not installed.";
        return QMap<QString, GameData>();
    }
    else{

    
        qCDebug(steamlogger) << "Discovering Steam games...";
        QMap<QString, QString> steamGames = getSteamGames();
        for (auto it = steamGames.constBegin(); it != steamGames.constEnd(); ++it) {
            QString path = it.value();
            QVariantMap vdfdata = VdfParser::parseFile(path);
            QVariantMap appState = vdfdata.value("appstate").toMap();
            if (appState.isEmpty()) {
                qCWarning(steamlogger)  << "Ugyldig eller tom manifestfil!";
                continue;
            }
            QFileInfo fileInfo(path);
            QString basePath = fileInfo.absolutePath(); 
            QString gamePath = basePath + "/common/" + appState.value("installdir", "UNKNOWNDIRDUDE").toString();
            QString GameName = appState.value("name", "Ukjent spill").toString();

            QDir dir(gamePath);
            if (!dir.exists()) {
                qCWarning(steamlogger) << "Game directory for " << GameName <<  "does not exist:" << gamePath;
                continue;
            }
            // Proton and the Steam Linux Runtime are compatibility tools (they ship a
            // toolmanifest.vdf), and 228980 installs the Steamworks redistributables: not games
            if (QFile::exists(gamePath + "/toolmanifest.vdf") || it.key() == QLatin1String("228980")) {
                continue;
            }
            QString gamePrefix = basePath + "/compatdata/" + appState.value("appid","0").toString();
            QDir pfxdir(gamePrefix);
            if (!pfxdir.exists()) {
                // native Linux games run without a Proton prefix
                gamePrefix.clear();
            }
            
            GameData data;
            data.launcher = "Steam";
            data.gameId = it.key();
            data.gameName = GameName;
            data.displayName = data.gameName;
            data.runner = "";
            data.installPath = gamePath;
            data.prefixPath = gamePrefix;
            m_games[data.displayName] = data;
            //qCDebug(steamlogger) << "Found Steam game:" << data.gameName << "(App ID:" << data.gameId << ")";
        }
    }
    return  m_games;
}


QMap<QString, QString> SteamScanner::getSteamGames()
{
    QMap<QString, QString> games;
    QString libraryConfig = findSteamLibraryConfig();
        
    if (libraryConfig.isEmpty()) {
        qCDebug(steamlogger) << "Could not find Steam library configuration";
        return games;
    }

    QFile file(libraryConfig);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(steamlogger) << "Failed to open Steam config:" << libraryConfig;
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
                                games[appId] = acfPath;//{ ,gameName};
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


bool SteamScanner::isLauncherInstalled() const
{
    // Check if steam command is in PATH (with Flatpak escape)
    QString launchCommand = "which steam";
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

QString SteamScanner::findSteamLibraryConfig()
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



QString SteamScanner::recursiveFind(const QDir &dir, int depth, int maxDepth)
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
