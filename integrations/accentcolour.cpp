// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/sensor.h"
#include <QCoreApplication>

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>
#include <KSandbox>
#include <QStandardPaths>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(ac)
Q_LOGGING_CATEGORY(ac, "integrations.AccentColour")

class AccentColourWatcher : public QObject
{
    Q_OBJECT
public:
    AccentColourWatcher(QObject *parent = nullptr)
        : QObject(parent)
    {
        auto sensor = new Sensor(this);
        sensor->setId("accentcolor");
        sensor->setName("Accent Color");
        sensor->setDiscoveryConfig("entity_category", "diagnostic");

        // Load initial state
        updateAccentColor(sensor);

        // Set up file watching (different for Flatpak vs native)
        setupFileWatching(sensor);
    }

private:
    void setupFileWatching(Sensor *sensor) {
        if (KSandbox::isFlatpak()) {
            // Flatpak: Use QFileSystemWatcher for absolute path
            QString configPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/kdeglobals";
            m_fileWatcher = new QFileSystemWatcher(this);
            m_fileWatcher->addPath(configPath);
            qCDebug(ac) << "Watching file:" << configPath;
            connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, 
                [this, sensor, configPath](const QString &path) {
                    qCDebug(ac) << "File changed:" << path;
                    updateAccentColor(sensor);
                    // TODO figure out why we need to do this to get updates after first change
                    m_fileWatcher->removePath(configPath);
                    m_fileWatcher->addPath(configPath);
                });
        } 
        else {
            // Native: Use KConfigWatcher (supports relative paths)
            m_configWatcher = KConfigWatcher::create(KSharedConfig::openConfig("kdeglobals"));
            
            connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, 
                [this, sensor](const KConfigGroup &group) {
                    if (group.name() == "General") {
                        updateAccentColor(sensor);
                    }
                });
        }
    }

    void updateAccentColor(Sensor *sensor) {
        KSharedConfig::Ptr config;
        
        if (KSandbox::isFlatpak()) {
            // Flatpak: Open with absolute path
            QString configPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/kdeglobals";
            config = KSharedConfig::openConfig(configPath);
        } else {
            // Native: Open normally
            config = KSharedConfig::openConfig("kdeglobals", KSharedConfig::CascadeConfig);
        }
        
        if (!config) {
            qCWarning(ac) << "Failed to open kdeglobals config file";
            return;
        }
        
        KConfigGroup general(config->group("General"));
        
        QString accentColor = general.readEntry("AccentColor","");
        QString lastUsedColor = general.readEntry("LastUsedCustomAccentColor");
        bool fromWallpaper = general.readEntry("accentColorFromWallpaper", false);
        
        QVariantMap attributes;
        
        // Set main state
        if (!accentColor.isEmpty()) {
            sensor->setState(rgbToHex(accentColor));
            attributes["has_accent"] = true;
            attributes["source"] = fromWallpaper ? "wallpaper" : "custom";
            setRgbAttributes(attributes, accentColor, "current");
        } else {
            // No accent color set (using theme default)
            sensor->setState("theme_default");
            attributes["has_accent"] = false;
            attributes["source"] = "theme";
            // Use KDE's default blue as fallback in attributes
            // TODO find theme colors from theme
            attributes["theme_default_color"] = "#3DAEE9";
            attributes["theme_default_rgb"] = "61,174,233";
        }
        
        // Always include last used custom color (if exists)
        if (!lastUsedColor.isEmpty()) {
            attributes["last_used_custom_hex"] = rgbToHex(lastUsedColor);
            setRgbAttributes(attributes, lastUsedColor, "last_used");
        }
        
        // Add from_wallpaper flag
        attributes["from_wallpaper"] = fromWallpaper;
        
        sensor->setAttributes(attributes);
    }

    QString rgbToHex(const QString &rgb) {
        QStringList parts = rgb.split(",");
        if (parts.size() != 3) return rgb;
        
        bool ok;
        int r = parts[0].toInt(&ok);
        if (!ok || r < 0 || r > 255) return rgb;
        int g = parts[1].toInt(&ok);
        if (!ok || g < 0 || g > 255) return rgb;
        int b = parts[2].toInt(&ok);
        if (!ok || b < 0 || b > 255) return rgb;
        
        return QString("#%1%2%3")
            .arg(r, 2, 16, QChar('0'))
            .arg(g, 2, 16, QChar('0'))
            .arg(b, 2, 16, QChar('0'));
    }
    
    void setRgbAttributes(QVariantMap &attributes, const QString &rgb, const QString &prefix) {
        QStringList parts = rgb.split(",");
        if (parts.size() == 3) {
            bool ok;
            int r = parts[0].toInt(&ok);
            int g = ok ? parts[1].toInt(&ok) : 0;
            int b = ok ? parts[2].toInt(&ok) : 0;
            
            if (ok) {
                QString attrPrefix = prefix.isEmpty() ? "" : prefix + "_";
                attributes[attrPrefix + "red"] = r;
                attributes[attrPrefix + "green"] = g;
                attributes[attrPrefix + "blue"] = b;
                attributes[attrPrefix + "rgb"] = QString("%1,%2,%3").arg(r).arg(g).arg(b);
            }
        }
    }

    KConfigWatcher::Ptr m_configWatcher;
    QFileSystemWatcher *m_fileWatcher = nullptr;
};

void setupAccentColour()
{
    new AccentColourWatcher(qApp);
}

REGISTER_INTEGRATION("AccentColour", setupAccentColour, true)

#include "accentcolour.moc"