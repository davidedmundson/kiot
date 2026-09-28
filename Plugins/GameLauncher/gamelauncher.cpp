// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later


#include "gamelauncher.h"
#include "launchers/FaugusScanner.h"
#include "launchers/heroicScanner.h"
#include "launchers/LutrisScanner.h"
#include "launchers/steamScanner.h"
#include "launchers/gamebase.h"
#include <QCoreApplication>
#include <QRegularExpression>
#include <KSharedConfig>
#include <KConfigGroup>
namespace
{
static const QRegularExpression invalidCharRegex("[^a-zA-Z0-9_-]");
}


using KIOTShared::Entities::BinarySensor;
using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger,GameLauncher)

GameLauncherPlugin::GameLauncherPlugin(QObject *parent)
    : QObject(parent)
{
    m_scanners.append(new SteamScanner());
    m_scanners.append(new HeroicScanner());
    m_scanners.append(new LutrisScanner());
    m_scanners.append(new FaugusScanner());
}
GameLauncherPlugin::~GameLauncherPlugin()
{
    for (auto scanner : m_scanners) {
        scanner->deleteLater();
        m_scanners.removeOne(scanner);
        scanner = nullptr;
    }
}

QString GameLauncherPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString GameLauncherPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}
QUrl GameLauncherPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}
QVersionNumber GameLauncherPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}


bool GameLauncherPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        auto appId = PlatformHelper::generateServiceName();
        
        bool hasFlatpakTalk = PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.freedesktop.Flatpak", "talk");
        if (!hasFlatpakTalk) {
            qCWarning(plugin_logger) << "Missing required Flatpak permission to talk to org.freedesktop.Flatpak.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --talk-name=org.freedesktop.Flatpak " + appId;
            return false;
        }

        bool hasSteamAccess = PlatformHelper::checkFlatpakFeature("Context", "filesystems", "xdg-data/Steam:ro");
        if (!hasSteamAccess) {
            qCWarning(plugin_logger) << "Missing required Flatpak permission for Steam launcher filesystem.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --filesystem=xdg-data/Steam:ro " + appId;
        }
        
        bool hasLutrisAccess = PlatformHelper::checkFlatpakFeature("Context", "filesystems", "xdg-data/lutris:ro");
        if (!hasLutrisAccess) {
            qCWarning(plugin_logger) << "Missing required Flatpak permission for Lutris launcher filesystem.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --filesystem=xdg-data/lutris:ro " + appId;
        }

        bool hasHeroicAccess = PlatformHelper::checkFlatpakFeature("Context", "filesystems", "xdg-config/heroic:ro");
        if (!hasHeroicAccess) {
            qCWarning(plugin_logger) << "Missing required Flatpak permission for Heroic launcher filesystem.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --filesystem=xdg-config/heroic:ro " + appId;
        }

        bool hasFaugusAccess = PlatformHelper::checkFlatpakFeature("Context", "filesystems", "xdg-data/faugus-launcher:ro");
        if (!hasFaugusAccess) {
            qCWarning(plugin_logger) << "Missing required Flatpak permission for Faugus launcher filesystem.";
            qCInfo(plugin_logger) << "To fix this, run:";
            qCInfo(plugin_logger) << "  flatpak --user override --filesystem=xdg-data/faugus-launcher:ro " + appId;
        }
    }


    bool launcherFound = false;
    for (auto *scanner : m_scanners) {
        if (scanner->isLauncherInstalled()) {   
            launcherFound = true;
            break; 
        }
    }
    
    if (!launcherFound) {
        qCWarning(plugin_logger) << "No supported game launchers found on the system.";
        return false;
    }

    return true;
}


bool GameLauncherPlugin::enabledByDefault()
{

    return checkCompatibility();
}

bool GameLauncherPlugin::startPlugin()
{
    if(m_select)
        stopPlugin();

    detectAllGames();
        
    if (m_games.isEmpty()) {
        qCWarning(plugin_logger) << "No games found from any launcher. GameLauncher integration disabled.";
        return false;
    }
    ensureConfig();
    createGameEntity();

    qCInfo(plugin_logger) << name() << " plugin started successfully";
    return true;
}

bool GameLauncherPlugin::stopPlugin()
{
    if(m_select)
    {
        m_select->deleteLater();
        m_select = nullptr;
    }
    m_games.clear();
    qCInfo(plugin_logger) << name() << " plugin stopped";
    return true;
}


void GameLauncherPlugin::detectAllGames()
{
    for (auto *scanner : m_scanners) {
        if (!scanner->isLauncherInstalled())
        {   
            qCDebug(plugin_logger) << "Launcher" << scanner->launcherName() << "is not installed SKIPPING";
            continue;
        }
        else {
            auto games = scanner->scanGames();
            for (const auto& [key, value] : games.asKeyValueRange()) {
                    m_games[value.launcher+" - "+value.gameName] = value;
            }
        }
    }
}


/**
 * @brief Creates a single select entity for all discovered games
 */
void GameLauncherPlugin::createGameEntity()
{
    m_select = new Select(this);
    m_select->setId("game_launcher");
    m_select->setName("Game Launcher");
    m_select->setDiscoveryConfig("icon", "mdi:gamepad-variant");
        
    QStringList options;
        
    auto config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    KConfigGroup settings(config, "gamelauncher");

        // Add games that are enabled in config
    for (auto it = m_games.constBegin(); it != m_games.constEnd(); ++it) {
        const QString &displayName = it.key();
      //  qCDebug(plugin_logger) << "Checking if" << sanitizeGameName(displayName) << "is enabled in config";
        
        if (settings.hasKey(sanitizeGameName(displayName))) {
            options.append(displayName);
        }
    }

    options = sortAlphabetically(options);
    options.prepend("Default");
    m_select->setOptions(options);
    m_select->setState("Default");
        
    connect(m_select, &Select::optionSelected, this, &GameLauncherPlugin::onOptionSelected);
        
    qCInfo(plugin_logger) << "Exposed" << options.size() << "games in select entity";
}


void GameLauncherPlugin::setToDefault()
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
QString GameLauncherPlugin::sanitizeGameName(const QString &gameName)
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
QList<QString> GameLauncherPlugin::sortAlphabetically(const QList<QString> &input)
{
    QList<QString> sorted = input;
    QCollator collator(QLocale::system());
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);

    std::sort(sorted.begin(), sorted.end(),[&collator](const QString &a, const QString &b) {
        return collator.compare(a, b) < 0;
    });

    return sorted;
}
/**
 * @brief Ensures configuration has entries for all discovered games
 */
void GameLauncherPlugin::ensureConfig()
{
    auto config = KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig );
    KConfigGroup settings(config, "gamelauncher");

    bool configChanged = false;

    // For each discovered game
    for (auto it = m_games.constBegin(); it != m_games.constEnd(); ++it) {
        const QString &displayName = it.key();
        QString configKey = sanitizeGameName(displayName);

        if (!settings.hasKey(configKey)) {
            settings.writeEntry(configKey, true);
            configChanged = true;
            qCDebug(plugin_logger) << "Added new game to config:" << configKey;
        }
    }
    const QStringList currentKeys = settings.keyList();

    for (const QString &configKey : currentKeys) {
        bool gameStillExists = false;

        for (auto it = m_games.constBegin(); it != m_games.constEnd(); ++it) {
            const QString &displayName = it.key();
            if (sanitizeGameName(displayName) == configKey) {
                gameStillExists = true;
                break;
            }
        }

        if (!gameStillExists) {
            settings.deleteEntry(configKey);
            configChanged = true;
            qCDebug(plugin_logger) << "Removed unavailable game from config:" << configKey;
        }
    }

    if (configChanged) {
        config->sync();
        qCDebug(plugin_logger) << "GameLauncher configuration updated with current games";
    }
}



/**
 * @brief Slot called when an option is selected
 * @param option The game identifier in format "Launcher - GameName"
 *
 * @details
 * Launches the specified game using the appropriate launcher's URI scheme.
 */
void GameLauncherPlugin::onOptionSelected(const QString &option)
{
    if (option == "Default" || !m_select) {
        return;
    }

        if (!m_games.contains(option)) {
            qCWarning(plugin_logger) << "Game not found in data:" << option;
            setToDefault();
            return;
        }

        GameData data = m_games[option];
        qCDebug(plugin_logger) << "Launching game:" << data.gameName << "(Launcher:" << data.launcher << ")";

        QString launchCommand;
        if (data.launcher == "Steam") {
            launchCommand =QStringLiteral("xdg-open steam://rungameid/%1").arg(data.gameId);
        } else if (data.launcher == "Heroic") {
            launchCommand =QStringLiteral("xdg-open heroic://launch?appName=%1&runner=%2").arg(data.gameId).arg(data.runner);
        } else if (data.launcher == "Lutris") {
            launchCommand =QStringLiteral("env LUTRIS_SKIP_INIT=1 lutris lutris:rungameid/%1").arg(data.gameId);
        } else if (data.launcher == "Faugus") {
            // TODO add faugust launch command
            qCWarning(plugin_logger) << "Faugus launcher not supported fully yet" << data.launcher;
            setToDefault();
            return;
        } else {
            qCWarning(plugin_logger) << "Unknown launcher:" << data.launcher;
            setToDefault();
            return;
        }

        QStringList args = QProcess::splitCommand(launchCommand);
        if (args.isEmpty()) {
            qCWarning(plugin_logger) << "Could not parse launch command:" << launchCommand;
            setToDefault();
            return;
        }

        QString program = args.takeFirst();

        if (PlatformHelper::isFlatpak()) {
            QProcess tempProcess;
            tempProcess.setProgram(program);
            tempProcess.setArguments(args);
    
            PlatformHelper::ProcessContext ctx = PlatformHelper::makeHostContext(tempProcess);
    
            bool success = QProcess::startDetached(ctx.program, ctx.arguments);
    
            if (success) {
                qCDebug(plugin_logger) << "Successfully launched game (detached):" << option;
    }        else {
                qCWarning(plugin_logger) << "Failed to launch game (detached):" << option;
            }
        } else {
            bool success = QProcess::startDetached(program, args);
    
            if (success) {
                qCDebug(plugin_logger) << "Successfully launched game (detached):" << option;
            } else {
                qCWarning(plugin_logger) << "Failed to launch game (detached):" << option;
            }
        }

        setToDefault();
    }
#include "gamelauncher.moc"