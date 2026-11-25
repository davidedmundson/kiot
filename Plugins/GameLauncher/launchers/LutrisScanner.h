// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef LUTRISSCANNER_H
#define LUTRISSCANNER_H

#include "gamebase.h"
#include <QJsonObject>
#include <QList>


class LutrisScanner : public GameBase{
public:
    explicit LutrisScanner(QObject *parent = nullptr);

    QString launcherName() const override { return "Lutris"; }
    bool isLauncherInstalled() const override;
    QMap<QString, GameData> scanGames() override;

    // Id of the game in Lutris' game-paths.json ({"<id>": "<full exe path>"}) whose path is the
    // game's exe; a relative exe (as the game's .yml may give it) must match whole path components
    static QString gameIdForExe(const QJsonObject &gamePaths, const QString &exe);

private:
    struct YamlInfo {
        QString gameId;
        QString gameName;
        QString prefixPath;
        QString runner;
        QString exepath;
        QString args;
    };
    QMap<QString, YamlInfo> getLutrisGames();
    YamlInfo parseYamlFile(const QString &yamlPath);
};

#endif // LUTRISSCANNER_H