// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core/core.h"
#include "Shared/entities/number.h"
#include "Shared/entities/select.h"
#include "Shared/platformhelper.h"


#include <PulseAudioQt/Context>
#include <PulseAudioQt/SinkInput>
#include <PulseAudioQt/Server>
#include <PulseAudioQt/Sink>
#include <PulseAudioQt/Source>
#include <PulseAudioQt/VolumeObject>

#include <QFileSystemWatcher>
#include <QFile>
#include <QTimer>
#include <QDir>

DEFINE_LOGGER(audio, integrations.Audio)



class Audio : public QObject
{
    Q_OBJECT

public:
    explicit Audio(QObject *parent = nullptr);

private slots:
    void updateSinks();
    void updateSources();
    void updateSinkInputs();
    void onSinkSelected(const QString &newOption);
    void onSinkInputSelected(const QString &newOption);
    void onSourceSelected(const QString &newOption);
    void onSourceVolumeChanged();
    void onSinkVolumeChanged();
    void onSinkInputVolumeChanged();
    void setSinkVolume(int v);
    void setSourceVolume(int v);
    void setSinkInputVolume(int v);
    

private:
    bool checkIfRaiseMaxVolumeEnabled();
    int paToPercent(qint64 v) const;
    qint64 percentToPa(int percent) const;

    QFileSystemWatcher *watcher = nullptr;
    Number *m_sinkVolume = nullptr;
    Number *m_sinkInputVolume = nullptr;
    Number *m_sourceVolume = nullptr;
    Select *m_sinkSelector = nullptr;
    Select *m_sourceSelector = nullptr;
    Select *m_sinkInputSelector = nullptr;

    PulseAudioQt::SinkInput *m_sinkInput = nullptr;
    PulseAudioQt::Sink *m_sink = nullptr;
    PulseAudioQt::Source *m_source = nullptr;
    PulseAudioQt::Context *m_ctx = nullptr;
};

// Constructor
Audio::Audio(QObject *parent)
    : QObject(parent)
{
    m_sinkVolume = new Number(this);
    m_sinkVolume->setId("output_volume");
    m_sinkVolume->setName("Output Volume");
    m_sinkVolume->setDiscoveryConfig("icon", "mdi:knob");
    if(checkIfRaiseMaxVolumeEnabled())
        m_sinkVolume->setRange(0, 150, 1, "%");
    else
        m_sinkVolume->setRange(0, 100, 1, "%");
    
    connect(m_sinkVolume, &Number::valueChangeRequested, this, &Audio::setSinkVolume);

    QString configPath = QDir::homePath() + "/.config/plasmaparc";
    if (QFile::exists(configPath))
    {
        watcher = new QFileSystemWatcher(this);
        watcher->addPath(configPath);
        connect(watcher, &QFileSystemWatcher::fileChanged, this, [this,configPath](const QString &){
            if (!this->watcher->files().contains(configPath)) {
                this->watcher->addPath(configPath);
            }
            if (QFile::exists(configPath)) {
                bool enabled = checkIfRaiseMaxVolumeEnabled();
                m_sinkVolume->setRange(0, enabled ? 150 : 100, 1, "%");
                m_sinkVolume->runtimeRegistration();
            }
        });
    }
    m_sourceVolume = new Number(this);
    m_sourceVolume->setId("input_volume");
    m_sourceVolume->setName("Input Volume");
    m_sourceVolume->setDiscoveryConfig("icon", "mdi:microphone");
    m_sourceVolume->setRange(0, 100, 1, "%");

    connect(m_sourceVolume, &Number::valueChangeRequested, this, &Audio::setSourceVolume);

    //App based volume controlelr
    m_sinkInputVolume = new Number(this);
    m_sinkInputVolume->setId("application_volume");
    m_sinkInputVolume->setName("Application Volume");
    m_sinkInputVolume->setDiscoveryConfig("icon", "mdi:knob");
    m_sinkInputVolume->setRange(0, 100, 1, "%");
    connect(m_sinkInputVolume, &Number::valueChangeRequested, this, &Audio::setSinkInputVolume);
    
    m_ctx = PulseAudioQt::Context::instance();
    if (!m_ctx || !m_ctx->isValid()) {
        qCWarning(audio) << "PulseAudio context not valid";
        return;
    }
    // Connect to the events for sink added/removed
    connect(m_ctx, &PulseAudioQt::Context::sinkAdded, this, &Audio::updateSinks);
    connect(m_ctx, &PulseAudioQt::Context::sinkRemoved, this, &Audio::updateSinks);
    // Connect to the events for source added/removed
    connect(m_ctx, &PulseAudioQt::Context::sourceAdded, this, &Audio::updateSources);
    connect(m_ctx, &PulseAudioQt::Context::sourceRemoved, this, &Audio::updateSources);

    auto *server = m_ctx->server();
    if (!server) {
        qCWarning(audio) << "No PulseAudio server";
        return;
    }

    // Sink selector and signal connection
    m_sinkSelector = new Select(this);
    m_sinkSelector->setId("volume_output_selector");
    m_sinkSelector->setDiscoveryConfig("icon", "mdi:volume-source");
    m_sinkSelector->setName("Output Device");
    connect(m_sinkSelector, &Select::optionSelected, this, &Audio::onSinkSelected);

    // SinkInput selector and signal connection
    m_sinkInputSelector = new Select(this);
    m_sinkInputSelector->setId("application_volume_selector");
    m_sinkInputSelector->setDiscoveryConfig("icon", "mdi:volume-source");
    m_sinkInputSelector->setName("Application Volume");
    connect(m_sinkInputSelector, &Select::optionSelected, this, &Audio::onSinkInputSelected);

    
    // Microphone selector and signal connection
    m_sourceSelector = new Select(this);
    m_sourceSelector->setId("volume_input_selector");
    m_sourceSelector->setDiscoveryConfig("icon", "mdi:microphone-settings");
    m_sourceSelector->setName("Input Device");
    connect(m_sourceSelector, &Select::optionSelected, this, &Audio::onSourceSelected);

    // Connect to signal when default sink changes
    connect(server, &PulseAudioQt::Server::defaultSinkChanged, this, &Audio::updateSinks);

    // Connect to signal when default source changes
    connect(server, &PulseAudioQt::Server::defaultSourceChanged, this, &Audio::updateSources);

    
    connect(m_ctx, &PulseAudioQt::Context::sinkInputAdded, this, [this]() {
        QTimer::singleShot(100, this, &Audio::updateSinkInputs);  
    });
    connect(m_ctx, &PulseAudioQt::Context::sinkInputRemoved, this, &Audio::updateSinkInputs);

    updateSinks();
    updateSources();
    updateSinkInputs();
}

//Updates the select entity for application volume controller with active apps
void Audio::updateSinkInputs()
{
    QStringList options;
    bool activeAppStillExists = false;

    for (const auto *input : m_ctx->sinkInputs()) {
        //qCDebug(audio) << "Sink input name" << input->name();
        //QVariant appNameVar = input->properties().value(QStringLiteral("application.name"));
        QString appName = input->name();
        if (appName.isEmpty() || appName == "Playback") {
            appName = input->properties().value(QStringLiteral("application.name")).toString();
        }
        
        if (!appName.isEmpty()) {
            options.append(appName);
            if (m_sinkInput && input == m_sinkInput) {
                activeAppStillExists = true;
            }
        }
    }

    if (options != m_sinkInputSelector->options()) {
        m_sinkInputSelector->setOptions(options);
    }

    if (!activeAppStillExists) {
        if (m_sinkInput) {
            disconnect(m_sinkInput, nullptr, this, nullptr);
        }
        
        if (!options.isEmpty()) {
            m_sinkInput = m_ctx->sinkInputs().first();
            m_sinkInputSelector->setState(options.first());
            connect(m_sinkInput, &PulseAudioQt::VolumeObject::volumeChanged, this, &Audio::onSinkInputVolumeChanged);
            onSinkInputVolumeChanged();
        } else {
            m_sinkInput = nullptr;
            m_sinkInputSelector->setState(QStringLiteral("Ingen aktive apper"));
            m_sinkInputVolume->setValue(0);
        }
    } else {
        m_sinkInputSelector->setState(m_sinkInputSelector->state());
    }
}

void Audio::updateSinks()
{
    auto sink = m_ctx->server()->defaultSink();
    // Fill the options of the select entity based on available sinks
    QStringList options;
    for (const auto *s : m_ctx->sinks())
        options.append(s->description());
    if (options != m_sinkSelector->options())
        m_sinkSelector->setOptions(options);

    // Disconnect from previous sink if any
    if (m_sink) {
        disconnect(m_sink, nullptr, this, nullptr);
    }

    m_sink = sink;

    if (m_sink) {
        m_sinkSelector->setState(m_sink->description());
        connect(m_sink, &PulseAudioQt::VolumeObject::volumeChanged, this, &Audio::onSinkVolumeChanged);
    }
    onSinkVolumeChanged();
}

void Audio::updateSources()
{
    auto source = m_ctx->server()->defaultSource();

    // Fill the options of the select entity based on available sinks
    QStringList options;
    for (const auto *s : m_ctx->sources())
        options.append(s->description());
    if (options != m_sourceSelector->options())
        m_sourceSelector->setOptions(options);

    // Disconnect from previous sink if any
    if (m_source) {
        disconnect(m_source, nullptr, this, nullptr);
    }

    m_source = source;

    if (m_source) {
        m_sourceSelector->setState(m_source->description());
        connect(m_source, &PulseAudioQt::VolumeObject::volumeChanged, this, &Audio::onSourceVolumeChanged);
    }
    onSourceVolumeChanged();
}
//COntrolled output device changed in HA
void Audio::onSinkSelected(const QString &newOption)
{
    if (!m_ctx)
        return;

    for (PulseAudioQt::Sink *sink : m_ctx->sinks()) {
        if (sink->description() == newOption) {
            qCDebug(audio) << "Setting sink to" << sink->description();
            sink->setDefault(true);
            return;
            ;
        }
    }
    qCWarning(audio) << "Sink not found:" << newOption;
}

//Controlled Application volume selected in HA
void Audio::onSinkInputSelected(const QString &newOption)
{
    if (!m_ctx)
        return;

    for (PulseAudioQt::SinkInput *sinkInput : m_ctx->sinkInputs()) {
            QString appName = sinkInput->name();
            if (appName.isEmpty() || appName == "Playback") {
                appName = sinkInput->properties().value(QStringLiteral("application.name")).toString();
            }
            if (appName == newOption) {
                if (m_sinkInput) {
                    disconnect(m_sinkInput, nullptr, this, nullptr);
                }
                m_sinkInput = sinkInput;
                if(m_sinkInput){
                    m_sinkInputSelector->setState(m_sinkInput->name());
                    connect(m_sinkInput, &PulseAudioQt::VolumeObject::volumeChanged, this, &Audio::onSinkInputVolumeChanged);
                }
                onSinkInputVolumeChanged();
                return;
            }
    }
    qCWarning(audio) << "SinkInput not found:" << newOption;
}

void Audio::onSourceSelected(const QString &newOption)
{
    if (!m_ctx)
        return;

    for (PulseAudioQt::Source *source : m_ctx->sources()) {
        if (source->description() == newOption) {
            qCDebug(audio) << "Setting source to" << source->description();
            source->setDefault(true);
            return;
        }
    }
    qCWarning(audio) << "Source not found:" << newOption;
}

void Audio::onSinkVolumeChanged()
{
    if (!m_sink)
        return;

    int percent = paToPercent(m_sink->volume());
    if (percent == m_sinkVolume->value())
        return;

    m_sinkVolume->setValue(percent);
    qCDebug(audio) << "Updated volume from system:" << percent << "%";
}

void Audio::onSinkInputVolumeChanged()
{
    if (!m_sinkInput)
        return;

    int percent = paToPercent(m_sinkInput->volume());
    if (percent == m_sinkInputVolume->value())
        return;

    m_sinkInputVolume->setValue(percent);
    qCDebug(audio) << "Updated application volume for" << m_sinkInput->name() << "  from system:" << percent << "%";
}

void Audio::onSourceVolumeChanged()
{
    if (!m_source)
        return;

    int percent = paToPercent(m_source->volume());
    if (percent == m_sourceVolume->value())
        return;

    m_sourceVolume->setValue(percent);
    qCDebug(audio) << "Updated volume from system:" << percent << "%";
}
void Audio::setSinkVolume(int v)
{
    if (!m_sink)
        return;
    if (v == m_sinkVolume->value())
        return;

    qint64 paVol = percentToPa(v);
    m_sink->setVolume(paVol);
    qCDebug(audio) << "Set volume to" << v << "%";
}

void Audio::setSinkInputVolume(int v)
{
    if (!m_sinkInput)
        return;
    if (v == m_sinkInputVolume->value())
        return;

    qint64 paVol = percentToPa(v);
    m_sinkInput->setVolume(paVol);
        
    qCDebug(audio) << "Set application volume for " << m_sinkInput->name() << " to" << v << "%";
}

void Audio::setSourceVolume(int v)
{
    if (!m_source)
        return;
    if (v == m_sourceVolume->value())
        return;

    qint64 paVol = percentToPa(v);
    m_source->setVolume(paVol);
    qCDebug(audio) << "Set volume to" << v << "%";
}
bool Audio::checkIfRaiseMaxVolumeEnabled()
{
    QString path = QStringLiteral("%1/.config/plasmaparc").arg(qgetenv("HOME"));
    QFile file(path);

    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false; 
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line == QLatin1String("RaiseMaximumVolume=true")) {
            return true;
        }
    }

    return false;
}
int Audio::paToPercent(qint64 v) const
{
    double p = (double)v / PulseAudioQt::normalVolume() * 100.0;
    return qRound(p);
}

qint64 Audio::percentToPa(int percent) const
{
    return qRound(PulseAudioQt::normalVolume() * (percent / 100.0));
}

void setupAudio()
{
    //TODO implement sandbox .flatpak-info parser to validate rights and give helpfull instructions for fix with flatseal
    new Audio(qApp);
}

REGISTER_INTEGRATION("Audio", setupAudio, true)
#include "audio.moc"