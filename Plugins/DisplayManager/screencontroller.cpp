// SPDX-FileCopyrightText: David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2026 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "screencontroller.h"

#include <KIOTShared/kiotshared.h>
#include <QTimer>
#include <cstdlib>

#include <kscreen/config.h>
#include <kscreen/configoperation.h>
#include <kscreen/getconfigoperation.h>
#include <kscreen/output.h>
#include <kscreen/setconfigoperation.h>
#include <kscreen/configmonitor.h>
#include <kscreen/mode.h>

using KIOTShared::Entities::Switch;
using KIOTShared::Entities::Select;
using KIOTShared::Entities::Number;
using KIOTShared::PlatformHelper;
// Ekstern logger definert i plugin-wrapperen eller her
Q_DECLARE_LOGGING_CATEGORY(disp_logger)

ScreenController::ScreenController(QObject *parent)
    : QObject(parent)
    , m_select(nullptr)
    , m_selectResolution(nullptr)
    , m_switchEnabled(nullptr)
    , m_switchPrimary(nullptr)
    , m_brightness(nullptr)
    , m_zoomScale(nullptr)
{
    setup();
    loadConfigFromSystem();

    connect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged, 
            this, &ScreenController::loadConfigFromSystem);
}

void ScreenController::setup()
{
    m_select = new Select(this);
    m_select->setId("displaylist");
    m_select->setName("Displays");
    m_select->setDiscoveryConfig("icon", "mdi:application-cog");

    QObject::connect(this, &ScreenController::screenNamesChanged, this, &ScreenController::onScreenNamesChanged);
    QObject::connect(m_select, &Select::optionSelected, this, &ScreenController::onOptionSelected);

    // Resolution select entity
    m_selectResolution = new Select(this);
    m_selectResolution->setId("displaylist_resolution");
    m_selectResolution->setName("Display Resolution");
    m_selectResolution->setDiscoveryConfig("icon", "mdi:monitor-screenshot");
    QObject::connect(m_selectResolution, &Select::optionSelected, this, &ScreenController::onResolutionSelected);

    // Enabled/disabled switch
    m_switchEnabled = new Switch(this);
    m_switchEnabled->setId("displaylist_enabled");
    m_switchEnabled->setName("Display Enabled");
    QObject::connect(m_switchEnabled, &Switch::stateChangeRequested, this, [this](bool state) {
        if (!m_activeoutput)
            return;
        if (m_activeoutput->isEnabled() == state)
            return;
        m_activeoutput->setEnabled(state);
        saveConfig();
    });

    // Primary screen switch
    m_switchPrimary = new Switch(this);
    m_switchPrimary->setId("displaylist_primary");
    m_switchPrimary->setName("Display Primary");
    QObject::connect(m_switchPrimary, &Switch::stateChangeRequested, this, [this](bool state) {
        if (!m_activeoutput || !m_config)
            return;
        if (!state) {
            m_switchPrimary->setState(true);
            return;
        }
        if (m_config->primaryOutput() != m_activeoutput) {
            m_config->setOutputPriority(m_activeoutput, 1);
            saveConfig();
        }
    });

    // Brightness scale
    m_brightness = new Number(this);
    m_brightness->setId("displaylist_brightness");
    m_brightness->setName("Display Brightness");
    m_brightness->setRange(0, 100, 1, "%");
    connect(m_brightness, &Number::valueChangeRequested, this, [this](int level) {
        if (!m_activeoutput)
            return;
        double result = static_cast<double>(level) / 100.0;
        qCDebug(disp_logger) << "Setting brightness to" << result;
        m_activeoutput->setBrightness(result);
        saveConfig();
    });

    // Zoom scale
    m_zoomScale = new Number(this);
    m_zoomScale->setId("displaylist_zoom");
    m_zoomScale->setName("Display Zoom");
    m_zoomScale->setRange(50, 300, 1, "%");
    
    connect(m_zoomScale, &Number::valueChangeRequested, this, [this](int level) {
        if (!m_activeoutput)
            return;
        double result = static_cast<double>(level) / 100.0;
        qCDebug(disp_logger) << "Setting Zoom to" << result;
        m_activeoutput->setScale(result);
        saveConfig();
    });
}

QStringList ScreenController::screenNames() const
{
    return m_screenNames;
}

void ScreenController::onOptionSelected(const QString &option)
{
    if (m_activeoutput) {
        disconnect(m_activeoutput.data(), nullptr, this, nullptr);
    }
    if (m_config) {
        for (auto output : m_config->outputs()) {
            if (QString("%1 (%2)").arg(output->model(), output->name()) != option)
                continue;
            m_activeoutput = output;
        }
    }
    if (!m_activeoutput)
        return;

    updateState();
}

void ScreenController::onResolutionSelected(const QString &option)
{
    if (!m_activeoutput)
        return;

    QString modeId = m_resolutionMap.value(option);
    if (modeId.isEmpty())
        return;

    if (m_activeoutput->currentModeId() != modeId) {
        qCDebug(disp_logger) << "Setting resolution mode ID:" << modeId << "for option:" << option;
        m_activeoutput->setCurrentModeId(modeId);
        saveConfig();
    }
}

void ScreenController::onScreenNamesChanged()
{
    if (!m_select)
        return;
    qCDebug(disp_logger) << "Screen names changed to:" << screenNames();
    m_select->setOptions(m_screenNames);

    if (!m_activeoutput && !m_screenNames.isEmpty()) {
        QTimer::singleShot(100, this, [this]() {
            m_select->setState(m_screenNames.first());
            onOptionSelected(m_screenNames.first());
        });
    }
}

void ScreenController::onPriorityChanged()
{
    if (!m_switchPrimary || !m_activeoutput)
        return;
    qCDebug(disp_logger) << "Primary screen changed.";
    bool isPrimary = (m_activeoutput == m_config->primaryOutput());
    if (m_switchPrimary->state() == isPrimary)
        return;
    m_switchPrimary->setState(isPrimary);
}

void ScreenController::loadConfigFromSystem()
{
    auto *operation = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoOptions, this);
    connect(operation, &KScreen::ConfigOperation::finished, this, [this, operation] {
        // Keep the current config and output when KScreen fails (e.g. while KWin restarts);
        // the next configurationChanged() reads it again.
        if (operation->hasError() || !operation->config()) {
            qCWarning(disp_logger) << "Failed to read the screen configuration:" << operation->errorString();
            return;
        }

        if (m_config) {
            disconnect(m_config.data(), nullptr, this, nullptr);
            KScreen::ConfigMonitor::instance()->removeConfig(m_config);
        }

        m_config = operation->config();
        
        if (m_config) {
            KScreen::ConfigMonitor::instance()->addConfig(m_config);
            connect(m_config.data(), &KScreen::Config::prioritiesChanged, this, &ScreenController::onPriorityChanged);
        }

        if (m_activeoutput) {
            int activeId = m_activeoutput->id();
            m_activeoutput.clear();
            for (auto output : m_config->outputs()) {
                if (output->id() == activeId) {
                    m_activeoutput = output;
                    break;
                }
            }
            if (m_activeoutput) {
                QObject::connect(m_activeoutput.data(), &KScreen::Output::isEnabledChanged, this, &ScreenController::updateState);
                QObject::connect(m_activeoutput.data(), &KScreen::Output::brightnessChanged, this, &ScreenController::updateState);
                QObject::connect(m_activeoutput.data(), &KScreen::Output::scaleChanged, this, &ScreenController::updateState);
                QObject::connect(m_activeoutput.data(), &KScreen::Output::currentModeIdChanged, this, &ScreenController::updateState);
            }
        }

        refreshScreenNames();
        updateState();
    });
}

void ScreenController::updateState()
{
    if (!m_activeoutput || !m_switchEnabled || !m_switchPrimary || !m_brightness || !m_zoomScale || !m_selectResolution || !m_config)
        return;
        
    m_switchEnabled->setState(m_activeoutput->isEnabled());
    m_switchPrimary->setState(m_activeoutput == m_config->primaryOutput());
    m_brightness->setValue(static_cast<int>(m_activeoutput->brightness() * 100));
    m_zoomScale->setValue(static_cast<int>(m_activeoutput->scale() * 100));

    m_resolutionMap.clear();
    QStringList modeOptions;
    auto modes = m_activeoutput->modes();
    
    for (auto mode : modes) {
        QString key = mode->id();
        QString value = mode->name();
        
        if (!modeOptions.contains(value)) {
            modeOptions.append(value);
            m_resolutionMap.insert(value, key);
        }
    }
    
    m_selectResolution->setOptions(modeOptions);

    QString currentModeId = m_activeoutput->currentModeId();
    for (auto it = m_resolutionMap.begin(); it != m_resolutionMap.end(); ++it) {
        if (it.value() == currentModeId) {
            m_selectResolution->setState(it.key());
            break;
        }
    }

    QVariantMap attributes;
    attributes["model"] = m_activeoutput->model();
    attributes["name"] = m_activeoutput->name();
    attributes["id"] = m_activeoutput->id();
    attributes["vendor"] = m_activeoutput->vendor();
    attributes["HDR"] = m_activeoutput->isHdrEnabled();
    attributes["enabled"] = m_activeoutput->isEnabled();
    attributes["primary"] = (m_activeoutput == m_config->primaryOutput());
    attributes["current_resolution_id"] = currentModeId;
    
    m_select->setAttributes(attributes);
}

void ScreenController::saveConfig()
{
    if (!m_config || !m_activeoutput)
        return;

    KScreen::ConfigPtr updatedConfig = m_config->clone();
    
    auto *operation = new KScreen::SetConfigOperation(updatedConfig, this);
    connect(operation, &KScreen::ConfigOperation::finished, this, [this, operation] {
        if (operation->hasError()) {
            qCWarning(disp_logger) << operation->errorString();
            return;
        }
        qCDebug(disp_logger) << "Screen Config saved successfully";
        loadConfigFromSystem();
    });
}

void ScreenController::refreshScreenNames()
{
    QStringList names;
    m_screenIds.clear();

    if (m_config) {
        for (auto output : m_config->outputs()) {
            names.append(QString("%1 (%2)").arg(output->model(), output->name()));
            m_screenIds.append(output->id());
        }
    }

    if (m_screenNames == names) {
        return;
    }

    m_screenNames = names;
    Q_EMIT screenNamesChanged();
}