// SPDX-FileCopyrightText: 2025-2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/select.h"

#include <KService>
#include <KServiceGroup>
#include <KApplicationTrader>
#include <KIO/ApplicationLauncherJob>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <QVariantMap>
#include <QTimer>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QCollator>
#include <QLocale>

#include <QSet>
#include <QDateTime>
#include <QRegularExpression>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(appla_logger)
Q_LOGGING_CATEGORY(appla_logger, "integration.AppLauncher")

namespace
{
static const QRegularExpression invalidCharRegex("[^a-zA-Z0-9_-]");
}

/**
 * @class AppLauncher
 * @brief Application launcher integration for KIOT
 *
 * @details
 * Discovers installed desktop applications using KDE's KService framework and exposes
 * them through a single Home Assistant select entity.
 * Categories can be toggled on/off in the configuration file to filter displayed applications.
 */
class AppLauncher : public QObject
{
    Q_OBJECT

public:
    explicit AppLauncher(QObject *parent = nullptr)
        : QObject(parent)
        , m_select(nullptr)
    {
        // Makes sure config file is up to date with every category available on system and returns those set to true
        QStringList allowedCategories = ensureConfigAndGetAllowedCategories();

        // Grabs every application that is in the allowed categories list
        discoverAllApplications(allowedCategories);
        // Stops of no application is found
        if (m_appList.isEmpty()) {
            qCWarning(appla_logger) << "No applications found matching the configured categories. AppLauncher disabled.";
            return;
        }

        // Creates the select entity for every app in HA
        createAppLauncherEntity();
    }

private slots:
    /**
     * @brief Slot called when an application option is selected in Home Assistant
     * @param option The localized application display name
     *
     * @details
     * Launches the target application using KIO::ApplicationLauncherJob, updates 
     * entity attributes with application metadata, and resets state back to "Default".
     */
    void onOptionSelected(const QString &option)
    {
        if (option == "Default" || !m_select) {
            return;
        }

        if (!m_appList.contains(option)) {
            qCWarning(appla_logger) << "Application not found in data:" << option;
            setToDefault();
            return;
        }

        AppData app = m_appList[option];

        KService::Ptr service = KService::serviceByDesktopName(app.desktopFileName);
        if (service) {
            qCDebug(appla_logger) << "Launching application:" << app.name;

            auto *job = new KIO::ApplicationLauncherJob(service);
            job->start();

            QVariantMap attributes = app.toVariantMap();
            attributes["last_launched_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            m_select->setAttributes(attributes);
        } else {
            qCWarning(appla_logger) << "Failed to resolve KService for application:" << app.name;
        }

        setToDefault();
    }

private:
    struct AppData {
        QString name;            // Lokalisert visningsnavn
        QString desktopFileName; // Skrivebordsfil-ID uten .desktop
        QString execCommand;     // Exec-kommando
        QString iconName;        // Freedesktop ikon-navn
        QStringList categories;  // Applikasjonskategorier

        QVariantMap toVariantMap() const {
            QVariantMap map;
            map["name"] = name;
            map["desktopFileName"] = desktopFileName;
            map["execCommand"] = execCommand;
            map["iconName"] = iconName;
            map["categories"] = QVariant::fromValue(categories);
            return map;
        }
    };

    void setToDefault()
    {
        if (m_select) {
            QTimer::singleShot(1000, this, [this]() {
                m_select->setState("Default");
            });
        }
    }

    /**
     * @brief Sanitizes category names for use as INI configuration keys
     */
    QString sanitizeCategoryName(const QString &category)
    {
        QString id = category.toLower();
        id.replace(invalidCharRegex, QStringLiteral("_"));
        if (!id.isEmpty() && id[0].isDigit()) {
            id.prepend("cat_");
        }
        return id;
    }

    /**
     * @brief Sorts a list of strings alphabetically according to system locale
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
     * @brief Reads application categories, syncs new categories to config, and returns enabled ones.
     * @return QStringList of active/enabled category names
     */
    QStringList ensureConfigAndGetAllowedCategories()
    {
        // 1. Samle alle unike kategorier fra tilgjengelige KServices
        QSet<QString> systemCategories;
        const KService::List services = KService::allServices();
        for (const KService::Ptr &service : services) {
            if (service->isApplication() && !service->noDisplay()) {
                const QStringList cats = service->categories();
                for (const QString &cat : cats) {
                    if (!cat.trimmed().isEmpty()) {
                        systemCategories.insert(cat.trimmed());
                    }
                }
            }
        }

        auto config = KSharedConfig::openConfig();
        KConfigGroup group(config, "AppLauncher");
     
        bool configChanged = false;
        QStringList allowedCategories;

        // Legg til nye oppdagede kategorier uten å overskrive eksisterende valg (default = true)
        for (const QString &cat : systemCategories) {
            QString configKey = sanitizeCategoryName(cat);
            if (!group.hasKey(configKey)) {
                group.writeEntry(configKey, true);
                configChanged = true;
                qCDebug(appla_logger) << "Added new application category to config:" << configKey << "= true";
            }

            // Sjekk om kategorien er aktivert
            if (group.readEntry(configKey, true)) {
                allowedCategories.append(cat);
            }
        }

        if (configChanged) {
            group.sync(); 
            qCInfo(appla_logger) << "AppLauncher config updated with system categories.";
        }
        

        return allowedCategories;
    }

    /**
     * @brief Scans system applications via KService filtered by category
     */
    void discoverAllApplications(const QStringList &allowedCategories)
    {
        QMap<QString, AppData> apps;
        const KService::List services = KService::allServices();

        for (const KService::Ptr &service : services) {
            if (!service->isApplication() || service->noDisplay()) {
                continue;
            }

            const QStringList serviceCategories = service->categories();
            bool matchesCategory = allowedCategories.isEmpty();

            if (!matchesCategory) {
                for (const QString &cat : allowedCategories) {
                    if (serviceCategories.contains(cat, Qt::CaseInsensitive)) {
                        matchesCategory = true;
                        break;
                    }
                }
            }

            if (!matchesCategory) {
                continue;
            }

            AppData data;
            data.name = service->name();
            data.desktopFileName = service->desktopEntryName();
            data.execCommand = service->exec();
            data.iconName = service->icon();
            data.categories = serviceCategories;

            apps[data.name] = data;
        }

        m_appList = apps;
        qCInfo(appla_logger) << "Total applications discovered:" << m_appList.size();
    }

    /**
     * @brief Creates the HA Select entity and populates options
     */
    void createAppLauncherEntity()
    {
        m_select = new Select(this);
        m_select->setId("app_launcher");
        m_select->setName("Application Launcher");
        m_select->setDiscoveryConfig("icon", "mdi:application-cog");

        QStringList options;
        for (auto it = m_appList.constBegin(); it != m_appList.constEnd(); ++it) {
            options.append(it.key());
        }

        options = sortAlphabetically(options);
        options.prepend("Default");
        m_select->setOptions(options);
        m_select->setState("Default");

        connect(m_select, &Select::optionSelected, this, &AppLauncher::onOptionSelected);

        qCInfo(appla_logger) << "Exposed" << options.size() - 1 << "applications in HA select entity";
    }

private:
    Select *m_select;
    QMap<QString, AppData> m_appList;
};

void setupAppLauncher()
{
    new AppLauncher(qApp);
}

REGISTER_INTEGRATION("AppLauncher", setupAppLauncher, true)

#include "applauncher.moc"