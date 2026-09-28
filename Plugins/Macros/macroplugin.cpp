// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file macroplugin.cpp
 * @brief Implementation of the KIOT plugin macroplugin.
 */

#include "macroplugin.h"
#include <QThread>
#include "virtualkeyboarddevice.h"

#include <QCoreApplication>

// Add the entities you need from the shared lib like this:
// using KIOTShared::Entities::Sensor;

using KIOTShared::PlatformHelper;

DEFINE_PLUGIN_LOGGER(plugin_logger, VirtualKeyboardDevice)

MacroPlugin::MacroPlugin(QObject *parent)
    : QObject(parent)
{
    qCWarning(plugin_logger) << "=================================================================";
    qCWarning(plugin_logger) << "WARNING:" << name() << "is an experimental plugin that uses direct uinput injection.";
    qCWarning(plugin_logger) << "This bypasses Wayland input security isolation and allows global";
    qCWarning(plugin_logger) << "keyboard/mouse control. Ensure your Home Assistant broker is secured.";
    qCWarning(plugin_logger) << "=================================================================";
}

MacroPlugin::~MacroPlugin()
{
    stopPlugin();
}

QString MacroPlugin::name() const
{
    return QString(PLUGIN_NAME).replace("\"", "");
}

QString MacroPlugin::description() const
{
    return QString(PLUGIN_DESCRIPTION).replace("\"", "") +QStringLiteral(" ") + QString(PLUGIN_DOMAIN).replace("\"", "");
}

QUrl MacroPlugin::url() const
{
    return QUrl(QString(PLUGIN_DOMAIN).replace("\"", ""));
}

QVersionNumber MacroPlugin::version() const
{
    QString version = QString(PLUGIN_VERSION).replace("\"", "");
    return QVersionNumber::fromString(version);
}

bool MacroPlugin::checkCompatibility()
{
    if (PlatformHelper::isFlatpak()) {
        qCWarning(plugin_logger) << "MacroPlugin requires direct character device node access (/dev/uinput), which is structurally blocked by the Flatpak sandbox.";
        qCInfo(plugin_logger) << "MacroPlugin is disabled in Flatpak environments to maintain 100% reliable native behavior.";
        return false;
    }
    if (!VirtualKeyboardDevice::hasUinputAccess()) {
        qCWarning(plugin_logger) << "No write access to /dev/uinput. Ensure the user is in the 'input' group on the host.";
        return false;
    }

    return true;
}

bool MacroPlugin::enabledByDefault()
{
    if(!checkCompatibility())
        return false;

    return false;
}

bool MacroPlugin::startPlugin()
{
    if(!checkCompatibility())
        return false;
    if(m_device || m_notify)
        stopPlugin();

    m_device = new VirtualKeyboardDevice( VirtualKeyboardDevice::SetupOptions{VirtualKeyboardDevice::EnableAbs,VirtualKeyboardDevice::EnableKey,VirtualKeyboardDevice::EnableRel}, this);
    if (!m_device->isReady()) {
        qCCritical(plugin_logger) << "Failed to create virtual keyboard device:" << m_device->lastError();
        return false;
    }

    m_notify = new Notify(this);
    m_notify->setId("odd_macros");
    m_notify->setName("Macros");
    m_notify->setDiscoveryConfig("icon", "mdi:keyboard");
    connect(m_notify, &Notify::notificationReceived, this, &MacroPlugin::notificationCallback);

    qCInfo(plugin_logger) << name() << "plugin started successfully";
    return true;
}

bool MacroPlugin::stopPlugin()
{
    if(m_device)
    {
        m_device->disconnect();
        m_device->deleteLater();
        m_device = nullptr;
    }
    if(m_notify)
    {
        disconnect(m_notify,nullptr,this,nullptr);
        m_notify->deleteLater();
        m_notify = nullptr;
    }
    qCInfo(plugin_logger) << name() << "plugin stopped";
    return true;
}

void MacroPlugin::notificationCallback(QByteArray message) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(message, &err);
    QString body;

    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        body = doc.object().value("message").toString();
    } else {
        body = QString::fromUtf8(message);
    }

    body = body.trimmed();
    if (body.isEmpty()) {
        m_device->printRegisteredKeys();
        return;
    }

    if (QString::compare(body, "PRINTKEYS", Qt::CaseInsensitive) == 0) {
        m_device->printRegisteredKeys();
        return;
    }

    // --- VALIDERING FØR KJØRING ---
    QString errorMsg;
    if (!validateSequence(body, errorMsg)) {
        qCWarning(plugin_logger) << "Makro-validator failed:" << errorMsg << "| Macro:" << body;
        return; // Avbryt slik at vi ikke sender noe tull til uinput!
    }

    // Hvis alt er OK, kjørsekvensen
    executeSequence(body);
}


bool MacroPlugin::validateSequence(const QString &sequenceStr, QString &errorMessage) {
    if (!m_device) {
        errorMessage = "Virtual keyboard device not initialized.";
        return false;
    }

    QStringList steps = sequenceStr.split(',', Qt::SkipEmptyParts);
    if (steps.isEmpty()) {
        errorMessage = "Macro sequence is empty.";
        return false;
    }

    for (int i = 0; i < steps.size(); ++i) {
        QString step = steps[i].trimmed();
        int colonIdx = step.indexOf(':');

        if (colonIdx == -1) {
            int code = m_device->keycodeFromName(step);
            if (code <= 0) {
                errorMessage =QStringLiteral("Step %1: Unknown value '%2'").arg(i + 1).arg(step);
                return false;
            }
            continue;
        }

        QString cmd = step.left(colonIdx).toLower().trimmed();
        QString val = step.mid(colonIdx + 1).trimmed();

        if (cmd == "down" || cmd == "press" || cmd == "up" || cmd == "release" || cmd == "click") {
            int code = m_device->keycodeFromName(val);
            if (code <= 0) {
                errorMessage =QStringLiteral("Step %1 (%2): Unknown key '%3'").arg(i + 1).arg(cmd).arg(val);
                return false;
            }
        } 
        else if (cmd == "delay") {
            bool ok = false;
            int ms = val.toInt(&ok);
            if (!ok || ms < 0) {
                errorMessage =QStringLiteral("Step %1 (delay): Invalid delay value '%2'").arg(i + 1).arg(val);
                return false;
            }
        } 
        else if (cmd == "type") {
            if (val.isEmpty()) {
                errorMessage =QStringLiteral("Step %1 (type): Empty text").arg(i + 1);
                return false;
            }
        } 
        else if (cmd == "mouse_move") {
            QStringList coords = val.split(';');
            if (coords.size() != 2) {
                errorMessage =QStringLiteral("Step %1 (mouse_move): needs this format X;Y (f.eks. 100;200)").arg(i + 1);
                return false;
            }
            bool ok1 = false, ok2 = false;
            coords[0].toInt(&ok1);
            coords[1].toInt(&ok2);
            if (!ok1 || !ok2) {
                errorMessage =QStringLiteral("Step %1 (mouse_move): Invalid cordinates '%2'").arg(i + 1).arg(val);
                return false;
            }
        } 
        else if (cmd == "scroll") {
            bool ok = false;
            val.toInt(&ok);
            if (!ok) {
                errorMessage =QStringLiteral("Step %1 (scroll): Invalid scroll number '%2'").arg(i + 1).arg(val);
                return false;
            }
        } 
        else {
            errorMessage =QStringLiteral("Step %1: Unknown commando '%2'").arg(i + 1).arg(cmd);
            return false;
        }
    }

    return true;
}
void MacroPlugin::executeSequence(const QString &sequenceStr) {
    if (!m_device) return;

    QStringList steps = sequenceStr.split(',', Qt::SkipEmptyParts);

    for (const QString &rawStep : steps) {
        QString step = rawStep.trimmed();
        int colonIdx = step.indexOf(':');

        if (colonIdx == -1) {
            // Backward compatibility: single key click without prefix
            int code = m_device->keycodeFromName(step);
            if (code > 0) {
                m_device->sendKeycode(code);
            }
            continue;
        }

        QString cmd = step.left(colonIdx).toLower().trimmed();
        QString val = step.mid(colonIdx + 1).trimmed();

        if (cmd == "down" || cmd == "press") {
            int code = m_device->keycodeFromName(val);
            if (code > 0) m_device->pressKeycode(code);

        } else if (cmd == "up" || cmd == "release") {
            int code = m_device->keycodeFromName(val);
            if (code > 0) m_device->releaseKeycode(code);

        } else if (cmd == "click") {
            int code = m_device->keycodeFromName(val);
            if (code > 0) m_device->sendKeycode(code);

        } else if (cmd == "delay") {
            bool ok = false;
            int ms = val.toInt(&ok);
            if (ok && ms > 0) {
                QThread::msleep(ms);
            }

        } else if (cmd == "type") {
            m_device->typeString(val);

        } else if (cmd == "mouse_move") {
            QStringList coords = val.split(';');
            if (coords.size() == 2) {
                int x = coords[0].toInt();
                int y = coords[1].toInt();
                m_device->moveMouse(x, y);
            }

        } else if (cmd == "scroll") {
            int steps = val.toInt();
            m_device->scrollWheel(steps);
        }

        // Micro-pause between steps to avoid event pacing issues in uinput/kernel
        QThread::msleep(10);
    }
}

#include "macroplugin.moc"