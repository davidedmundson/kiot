// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core/core.h"
#include "Shared/entities/entities.h"
#include <QCoreApplication>

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>

class AccentColourWatcher : public QObject
{
    Q_OBJECT
public:
    AccentColourWatcher(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_sensor = new Sensor(this);
        m_sensor->setId("accentcolor");
        m_sensor->setName("Accent Color");

        KConfigGroup config(KSharedConfig::openConfig("kdeglobals")->group("General"));
        updateAccentColor(config);
        m_watcher = KConfigWatcher::create(KSharedConfig::openConfig("kdeglobals"));

        QObject::connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
            if (group.name() != "General") {
                return;
            }
            updateAccentColor(group);
        });
    }

private:
    void updateAccentColor(const KConfigGroup &config) {
        QString accentColor = config.readEntry("AccentColor","");
        QString lastUsedColor = config.readEntry("LastUsedCustomAccentColor","");
        bool fromWallpaper = config.readEntry("accentColorFromWallpaper", false);
        
        QVariantMap attributes;
        
        if (!accentColor.isEmpty()) {
            m_sensor->setState(rgbToHex(accentColor));
            attributes["has_accent"] = true;
            attributes["source"] = fromWallpaper ? "wallpaper" : "custom";
            setRgbAttributes(attributes, accentColor, "current");
        } else {
            m_sensor->setState("theme_default");
            attributes["has_accent"] = false;
            attributes["source"] = "theme";
            // Use KDE's default blue as fallback in attributes
            // TODO find theme colors from theme
            attributes["theme_default_color"] = "#3DAEE9";
            attributes["theme_default_rgb"] = "61,174,233";
        }
        if (!lastUsedColor.isEmpty()) {
            attributes["last_used_custom_hex"] = rgbToHex(lastUsedColor);
            setRgbAttributes(attributes, lastUsedColor, "last_used");
        }
        attributes["from_wallpaper"] = fromWallpaper;
        
        m_sensor->setAttributes(attributes);
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
    Sensor *m_sensor;
    KConfigWatcher::Ptr m_watcher;
};

void setupAccentColour()
{
    new AccentColourWatcher(qApp);
}

REGISTER_INTEGRATION("AccentColour", setupAccentColour, true)

#include "accentcolour.moc"
