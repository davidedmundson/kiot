// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file update.h
 * @brief Update entity for Home Assistant integration
 *
 * @details
 * This header defines the Update class which implements an update entity
 * for Home Assistant. Update entities represent firmware or software updates
 * that can be monitored and installed from Home Assistant.
 *
 * The update entity supports:
 * - Reporting installed version
 * - Reporting latest available version
 * - Starting installation process from Home Assistant
 * - Progress tracking during installation
 * - Additional metadata (title, release notes, etc.)
 *
 * Based on Home Assistant's MQTT update integration documentation:
 * https://www.home-assistant.io/integrations/update.mqtt/
 */

#pragma once
#include "entity.h"

/**
 * @class Update
 * @brief Update entity for firmware/software updates
 *
 * @details
 * The Update class represents an update entity in Home Assistant, used for
 * monitoring and installing firmware or software updates. It supports
 * bidirectional communication: update information is reported to Home
 * Assistant, and installation commands from Home Assistant trigger
 * installation requests in KIOT.
 *
 * Common use cases:
 * - Kiot update from home assistant
 *
 * @note Inherits from Entity to leverage MQTT discovery and topic management.
 */
class Update : public Entity
{
    Q_OBJECT
public:
    /**
     * @brief Constructs an Update entity
     * @param parent Parent QObject for memory management (optional)
     */
    Update(QObject *parent = nullptr);

    /**
     * @brief Sets the installed version
     * @param version Installed version string
     *
     * @details
     * Sets the currently installed version and publishes it to Home Assistant.
     * The version should be in a format that can be compared (e.g., "1.2.3").
     */
    void setInstalledVersion(const QString &version);

    /**
     * @brief Sets the latest available version
     * @param version Latest available version string
     *
     * @details
     * Sets the latest available version and publishes it to Home Assistant.
     * This indicates what version is available for installation.
     */
    void setLatestVersion(const QString &version);

    /**
     * @brief Sets the update title
     * @param title Update title/name
     *
     * @details
     * Sets a descriptive title for the update (e.g., "Device Firmware").
     * This helps differentiate between the entity name and the update itself.
     */
    void setTitle(const QString &title);

    /**
     * @brief Sets the release summary
     * @param summary Brief summary of the update (max 255 chars)
     *
     * @details
     * Sets a brief summary of the release notes or changelog.
     * This should be a concise description of the update.
     */
    void setReleaseSummary(const QString &summary);

    /**
     * @brief Sets the release URL
     * @param url URL to full release notes
     *
     * @details
     * Sets a URL pointing to the complete release notes or changelog.
     */
    void setReleaseUrl(const QString &url);

    /**
     * @brief Sets the entity picture URL
     * @param url URL to an image for the update
     *
     * @details
     * Sets a URL pointing to an image that represents the update.
     * This image will be displayed as the entity picture in Home Assistant.
     */
    void setEntityPicture(const QString &url);

    /**
     * @brief Sets the update progress state
     * @param inProgress Whether an update is currently in progress
     *
     * @details
     * Sets whether an update installation is currently in progress.
     * When true, Home Assistant will show the update as "installing".
     */
    void setInProgress(bool inProgress);

    /**
     * @brief Sets the update progress percentage
     * @param percentage Progress percentage (0-100), or -1 to clear
     *
     * @details
     * Sets the current installation progress percentage.
     * Use -1 to clear/reset the progress indicator.
     */
    void setUpdatePercentage(int percentage);

    /**
     * @brief Gets the current installed version
     * @return QString Currently installed version
     */
    QString installedVersion() const
    {
        return m_installedVersion;
    }

    /**
     * @brief Gets the latest available version
     * @return QString Latest available version
     */
    QString latestVersion() const
    {
        return m_latestVersion;
    }

    /**
     * @brief Gets whether an update is in progress
     * @return bool True if update is in progress
     */
    bool inProgress() const
    {
        return m_inProgress;
    }

    /**
     * @brief Gets the current update progress percentage
     * @return int Progress percentage (0-100), or -1 if not set
     */
    int updatePercentage() const
    {
        return m_updatePercentage;
    }

    /**
     * @brief Publishes the current update state to Home Assistant
     *
     * @details
     * Publishes all current update information (installed version,
     * latest version, progress, etc.) to Home Assistant as a JSON payload.
     * This should be called after setting multiple properties to ensure
     * all changes are published together.
     */
    void publishState();

Q_SIGNALS:
    /**
     * @brief Signal emitted when installation is requested from Home Assistant
     *
     * @details
     * Emitted when Home Assistant sends a command to start the update installation.
     * Integrations should connect to this signal to implement the actual
     * installation logic for the update.
     */
    void installRequested();

protected:
    /**
     * @brief Initializes the update entity
     *
     * @details
     * Overrides Entity::init() to set up update-specific configuration,
     * including command topic subscription for receiving installation
     * requests from Home Assistant.
     */
    void init() override;

private:
    /** @private Currently installed version */
    QString m_installedVersion;

    /** @private Latest available version */
    QString m_latestVersion;

    /** @private Update title/name */
    QString m_title;

    /** @private Release summary */
    QString m_releaseSummary;

    /** @private Release URL */
    QString m_releaseUrl;

    /** @private Entity picture URL */
    QString m_entityPicture;

    /** @private Whether update is in progress */
    bool m_inProgress = false;

    /** @private Update progress percentage (-1 = not set) */
    int m_updatePercentage = -1;
};