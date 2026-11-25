// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "LutrisScanner.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <qloggingcategory.h>

DEFINE_PLUGIN_LOGGER(lutrislogger, GameLauncher.utrisScanner)

LutrisScanner::LutrisScanner(QObject *parent) : GameBase(parent)
{

}
bool LutrisScanner::isLauncherInstalled() const
{
    // Check if lutris command is in PATH
    QString launchCommand = "which lutris";
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

    QStringList desktopPaths = {
        QDir::homePath() + "/.local/share/applications/lutris.desktop",
        "/usr/share/applications/lutris.desktop",
        "/var/lib/flatpak/exports/share/applications/net.lutris.Lutris.desktop"
    };

    for (const QString &desktopPath : desktopPaths) {
        if (QFile::exists(desktopPath)) {
            return true;
        }
    }

    QString lutrisHome = QDir::homePath() + "/.local/share/lutris";
    if (QDir(lutrisHome).exists()) {
        return true;
    }

    return false;
}

LutrisScanner::YamlInfo LutrisScanner::parseYamlFile(const QString &yamlPath)
{
    YamlInfo info;
    QFile yamlFile(yamlPath);
    if (!yamlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return info;
    }

    QString yamlContent = QString::fromUtf8(yamlFile.readAll());
    yamlFile.close();

    // Parse name
    QRegularExpression nameRegex("name:\\s*\"?([^\"\\n]+)\"?");
    QRegularExpressionMatch nameMatch = nameRegex.match(yamlContent);
    if (nameMatch.hasMatch()) {
        info.gameName = nameMatch.captured(1).trimmed();
    }

    // Parse prefix
    QRegularExpression prefixRegex("prefix:\\s*\"?([^\"\\n]+)\"?");
    QRegularExpressionMatch prefixMatch = prefixRegex.match(yamlContent);
    if (prefixMatch.hasMatch()) {
        info.prefixPath = prefixMatch.captured(1).trimmed();
    }

    // Parse runner
    QRegularExpression runnerRegex("runner:\\s*\"?([^\"\\n]+)\"?");
    QRegularExpressionMatch runnerMatch = runnerRegex.match(yamlContent);
    if (runnerMatch.hasMatch()) {
        info.runner = runnerMatch.captured(1).trimmed();
    }
    QRegularExpression exeRegex("exe:\\s*\"?([^\"\\n]+)\"?");
    QRegularExpressionMatch exeMatch = exeRegex.match(yamlContent);
    if (exeMatch.hasMatch()) {
         info.exepath = exeMatch.captured(1).trimmed();
    }
    QRegularExpression argsRegex("args:\\s*\"?([^\"\\n]+)\"?");
    QRegularExpressionMatch argsMatch = argsRegex.match(yamlContent);
    if (argsMatch.hasMatch()) {
         info.args = argsMatch.captured(1).trimmed();
    }
    return info;
}

QMap<QString, GameData> LutrisScanner::scanGames()
{

    if (isLauncherInstalled()) {
        qCDebug(lutrislogger) << "Discovering Lutris games...";
        QMap<QString, YamlInfo> lutrisGames = getLutrisGames();
        for (auto it = lutrisGames.constBegin(); it != lutrisGames.constEnd(); ++it) {
            GameData data;
            YamlInfo info = it.value();
            QFileInfo fileInfo(info.exepath);
            if(info.gameName.isEmpty())
            {
                continue;
            }
            data.launcher = launcherName();
            data.gameId = info.gameId;
            data.gameName = it.key();
            data.displayName =QStringLiteral("%1").arg(data.gameName);
            data.runner = "";
            data.installPath = fileInfo.absolutePath();
            data.prefixPath = info.prefixPath;    
            data.exePath = info.exepath;
            data.launchOptions = info.args;
            m_games[data.displayName] = data;
          //  qCDebug(lutrislogger) << "Found Lutris game:" << data.gameName << "(game ID:" << data.prefixPath << ")";
        }
    }
    return m_games;
}


QString LutrisScanner::gameIdForExe(const QJsonObject &gamePaths, const QString &exe)
{
    // no exe: in the .yml (e.g. Steam or DOSBox games): no id
    if (exe.trimmed().isEmpty()) {
        return QString();
    }
    const QString exePath = QDir::cleanPath(exe.trimmed());
    for (auto it = gamePaths.begin(); it != gamePaths.end(); ++it) {
        const QString path = QDir::cleanPath(it.value().toString());
        if (QDir::isAbsolutePath(exePath) ? path == exePath : path.endsWith(QLatin1Char('/') + exePath)) {
            return it.key();
        }
    }
    return QString();
}

QMap<QString, LutrisScanner::YamlInfo> LutrisScanner::getLutrisGames()
{
    QMap<QString, YamlInfo> games;

    QString gamePathsPath;
    QString gamesDir;

    QString nativeCache = QDir::homePath() + "/.cache/lutris/game-paths.json";
    QString nativeGames = QDir::homePath() + "/.local/share/lutris/games";

    QString flatpakCache = QDir::homePath() + "/.var/app/net.lutris.Lutris/cache/lutris/game-paths.json";
    QString flatpakGames = QDir::homePath() + "/.var/app/net.lutris.Lutris/data/lutris/games";

    if (QFile::exists(flatpakCache) && QDir(flatpakGames).exists()) {
        gamePathsPath = flatpakCache;
        gamesDir = flatpakGames;
    } else if (QFile::exists(nativeCache) && QDir(nativeGames).exists()) {
        gamePathsPath = nativeCache;
        gamesDir = nativeGames;
    } else {
        return QMap<QString, YamlInfo>();
    }

    if (!QFile::exists(gamePathsPath)) {
        qCDebug(lutrislogger) << "Lutris game-paths.json not found at:" << gamePathsPath;
        return games;
    }

    QFile gamePathsFile(gamePathsPath);
    if (!gamePathsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lutrislogger) << "Could not open game-paths.json:" << gamePathsPath;
        return games;
    }

    QByteArray gamePathsData = gamePathsFile.readAll();
    gamePathsFile.close();

    QJsonDocument gamePathsDoc = QJsonDocument::fromJson(gamePathsData);
    if (gamePathsDoc.isNull() || !gamePathsDoc.isObject()) {
        qCWarning(lutrislogger) << "Invalid JSON in game-paths.json";
        return games;
    }

    QJsonObject gamePaths = gamePathsDoc.object();

    QDirIterator yamlIt(gamesDir, QStringList() << "*.yml", QDir::Files);
    QMap<QString, YamlInfo> yamlGames; 

    while (yamlIt.hasNext()) {
        YamlInfo inf = parseYamlFile(yamlIt.next());
        
        inf.gameId = gameIdForExe(gamePaths, inf.exepath);
        yamlGames[inf.gameName] = inf;
    }
    return yamlGames;
}