// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "FaugusScanner.h"
#include "gamebase.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <qloggingcategory.h>
#include <qobject.h>


DEFINE_PLUGIN_LOGGER(fauguslogger, GameLauncher.FaugusScanner)

FaugusScanner::FaugusScanner(QObject *parent) : GameBase(parent)
{

}
bool FaugusScanner::isLauncherInstalled() const
{
    // Check if faugus command is in PATH
    QString launchCommand = "which faugus-launcher";
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
        // exitCode() is 0 too when `which` could not be started (not installed)
        if (process.waitForFinished() && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
            return true;
        }       
    }

    // Check for Faugus desktop file, todo double check flatpak name
    QStringList desktopPaths = {
        QDir::homePath() + "/.local/share/applications/io.github.Faugus.faugus-launcher.desktop",
        "/usr/share/applications/io.github.Faugus.faugus-launcher.desktop",
        "/var/lib/flatpak/exports/share/applications/io.github.Faugus.faugus-launcher.desktop"
    };

    for (const QString &desktopPath : desktopPaths) {
        if (QFile::exists(desktopPath)) {
            return true;
        }
    }

    // Check for Faugus installation directory
    QString FaugusHome = QDir::homePath() + "/.local/share/faugus-launcher/";
    
    if (QDir(FaugusHome).exists()) {
        return true;
    }
    FaugusHome = QDir::homePath() + ".config/faugus-launcher/";
    if (QDir(FaugusHome).exists()) {
        return true;
    }
    return false;
}

QMap<QString, GameData> FaugusScanner::scanGames()
{
    m_games.clear();

    if (!isLauncherInstalled()) {
        return m_games;
    }
    qCDebug(fauguslogger) << launcherName() << " scanning for installed games";
    QString faugusGamesFile = QDir::homePath() + "/.local/share/faugus-launcher/games.json";
    if(!QFile(faugusGamesFile).exists())
    {
        qCDebug(fauguslogger) << launcherName() << "Fant ikke fil:" << faugusGamesFile << "looking in flatpak paths";
        faugusGamesFile = QDir::homePath() + "/.var/app/io.github.Faugus.faugus-launcher/data/faugus-launcher/games.json";
        if(!QFile(faugusGamesFile).exists())
        {
            qCDebug(fauguslogger) << launcherName() << "No detected games found";
            return m_games;
        }
    }
    QFile file(faugusGamesFile);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCDebug(fauguslogger) << launcherName() << "Kunne ikke åpne fil:" << faugusGamesFile;
        return m_games;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError) {
        qCDebug(fauguslogger) << launcherName() << "JSON Parse Error:" << error.errorString();
        return m_games;
    }

    if (!doc.isArray()) {
        qCWarning(fauguslogger) << launcherName() << "Forventet en JSON Array i rot, men fikk noe annet.";
        return m_games;
    }

    QJsonArray gamesArray = doc.array();

    for (const QJsonValue &value : gamesArray) {
        //qCDebug(fauguslogger) << launcherName() << " json object " << value;
        if (!value.isObject()) {
            continue;
        }

        QJsonObject gameObj = value.toObject();

        QString gameId = gameObj["gameid"].toString();
        QString title  = gameObj["title"].toString();
        QString launch_arguments = gameObj["launch_arguments"].toString("");
        QString game_arguments = gameObj["game_arguments"].toString("");
        QString iconPath = gameObj["icon"].toString("");
        if (title.isEmpty()) {
            continue;
        }

        GameData data;
        data.launcher    = launcherName(); // "Faugus"
        data.gameId      = gameId;
        data.gameName    = title;
        data.displayName = title;
        data.runner      = gameObj["runner"].toString("");
        data.prefixPath  = gameObj["prefix"].toString("");
        data.iconPath = iconPath;
        data.envVariables = launch_arguments;
        data.launchOptions = game_arguments;
        data.exePath     = gameObj["path"].toString(""); 
        
        QFileInfo exeInfo(data.exePath);
        data.installPath = exeInfo.absolutePath();
        m_games[gameId] = data; 
    }

    return m_games;
}
