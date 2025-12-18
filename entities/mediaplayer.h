// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file mediaplayer.h
 * @brief Media Player entity for Home Assistant integration
 * 
 * @details
 * This header defines the MediaPlayer class which implements a media
 * player entity for Home Assistant. Media player entities provide comprehensive
 * media control functionality including play/pause, volume control, track
 * navigation, and media information display.
 * 
 * This entity is designed to work with the custom MQTT Media Player integration
 * for Home Assistant, providing full media control capabilities for desktop
 * media players through KIOT.
 */

#pragma once

#include "entity.h"
#include <QStringList>
#include <QVariantMap>
#include <QObject>
#include <QMqttMessage>

/**
 * @class MediaPlayer
 * @brief Media player entity for comprehensive media control
 * 
 * @details
 * The MediaPlayer class represents a media player entity in Home Assistant,
 * used for controlling media playback on the desktop. It supports the full
 * range of media player functionality including playback control, volume
 * adjustment, track navigation, and media information display.
 * 
 * This entity works with the custom MQTT Media Player integration:
 * https://github.com/bkbilly/mqtt_media_player
 *
 * I made some updates to the mqtt media player here that supports seek to
 * https://github.com/TheOddPirate/mqtt_media_player 
 *
 * Common use cases:
 * - MPRIS media player control (via separate integration)
 * - Media playback status monitoring and controlling
 * 
 * @note Inherits from Entity to leverage MQTT discovery and topic management.
 */
class MediaPlayer : public Entity
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a MediaPlayer
     * @param parent Parent QObject for memory management (optional)
     */
    explicit MediaPlayer(QObject *parent = nullptr);

    /**
     * @brief Sets the complete media player state
     * @param info Map containing all media player state information
     * 
     * @details
     * Sets the complete state of the media player including:
     * - Playback state (playing, paused, stopped)
     * - Current media information (title, artist, album, etc.)
     * - Volume level
     * - Available players list
     * - Any other media player attributes
     * 
     * The state is automatically published to Home Assistant.
     */
    void setState(const QVariantMap &info);
    
    /**
     * @brief Gets the current media player state
     * @return QVariantMap Current media player state information
     */
    QVariantMap state() const;


protected:
    /**
     * @brief Initializes the media player entity
     * 
     * @details
     * Overrides Entity::init() to set up media player-specific configuration,
     * including subscription to multiple command topics for various media
     * control commands from Home Assistant.
     */
    void init() override;
    
    /**
     * @brief Publishes the current state to Home Assistant
     * 
     * @details
     * Internal method that publishes the complete media player state
     * to the entity's MQTT state topic.
     */
    void publishState();

private slots:
    // Command handlers for MQTT messages from Home Assistant
    void onPlayCommand(const QString &payload);
    void onPauseCommand(const QString &payload);
    void onPlayPauseCommand(const QString &payload);
    void onStopCommand(const QString &payload);
    void onNextCommand(const QString &payload);
    void onPreviousCommand(const QString &payload);
    void onSetVolumeCommand(const QString &payload);
    void onPlayMediaCommand(const QString &payload);
    void onPositionCommand(const QString &payload);
public slots:
    /**
     * @brief Starts media playback
     */
    void play();
    
    /**
     * @brief Pauses media playback
     */
    void pause();
    
    /**
     * @brief Stops media playback
     */
    void stop();
    
    /**
     * @brief Skips to next track
     */
    void next();
    
    /**
     * @brief Returns to previous track
     */
    void previous();
    
    /**
     * @brief Sets the volume level
     * @param volume Volume level (0.0 to 1.0 or 0 to 100 depending on implementation)
     */
    void setVolume(qreal volume);

signals:
    /**
     * @brief Signal emitted when media player state changes
     * @param newState New media player state information
     */
    void stateChanged(QVariantMap newState);
    
    /**
     * @brief Signal emitted when play is requested from Home Assistant
     */
    void playRequested();
    
    /**
     * @brief Signal emitted when pause is requested from Home Assistant
     */
    void pauseRequested();
    
    /**
     * @brief Signal emitted when stop is requested from Home Assistant
     */
    void stopRequested();
    
    /**
     * @brief Signal emitted when next track is requested from Home Assistant
     */
    void nextRequested();
    
    /**
     * @brief Signal emitted when previous track is requested from Home Assistant
     */
    void previousRequested();
    
    /**
     * @brief Signal emitted when volume change is requested from Home Assistant
     * @param volume Requested volume level
     */
    void volumeChanged(double volume);
    
    /**
     * @brief Signal emitted when media playback is requested from Home Assistant
     * @param payload Media playback request payload (e.g., URL, media identifier)
     */
    void playMediaRequested(const QString &payload);
    
    /**
     * @brief Signal emitted when position change is requested from Home Assistant
     * @param payload Position request payload in seconds
     */
    void positionChanged(const qint64 &payload);

    

private:
    /** @private Current media player state information */
    QVariantMap m_state;

};