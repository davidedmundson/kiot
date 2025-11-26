#include "core.h"

#include <QCoreApplication>
#include <QObject>
#include <QDebug>

#include <KF6/KF6PulseAudioQt/PulseAudioQt/Context>
#include <KF6/KF6PulseAudioQt/PulseAudioQt/Server>
#include <KF6/KF6PulseAudioQt/PulseAudioQt/Sink>
#include <KF6/KF6PulseAudioQt/PulseAudioQt/VolumeObject>

class Audio : public QObject
{
    Q_OBJECT

public:
    explicit Audio(QObject *parent = nullptr);

private slots:
    void onDefaultSinkChanged(PulseAudioQt::Sink *sink);
    void onVolumeChanged();
    void setVolume(int v);

private:
    int paToPercent(qint64 v) const;
    qint64 percentToPa(int percent) const;

    Number *m_sinkVolume = nullptr;
    //TODO implement sensors/selector for sink/source
    PulseAudioQt::Sink *m_sink = nullptr;

};

// Constructor
Audio::Audio(QObject *parent)
    : QObject(parent)
{
    m_sinkVolume = new Number(this);
    m_sinkVolume->setId("volume");
    m_sinkVolume->setName("System Volume");
    m_sinkVolume->setRange(0, 100, 1, "%");

    connect(m_sinkVolume, &Number::valueChangeRequested,
            this, &Audio::setVolume);

    auto ctx = PulseAudioQt::Context::instance();
    if (!ctx || !ctx->isValid()) {
        qWarning() << "Audio: PulseAudio context not valid";
        return;
    }

    auto *m_server = ctx->server();
    if (!m_server) {
        qWarning() << "Audio: No PulseAudio server";
        return;
    }

    connect(m_server, &PulseAudioQt::Server::defaultSinkChanged,
            this, &Audio::onDefaultSinkChanged);

    auto *initial = m_server->defaultSink();
    if (initial) {
        onDefaultSinkChanged(initial);
    } else {
        qWarning() << "Audio: default sink not ready, waiting for signal";
    }
}

void Audio::onDefaultSinkChanged(PulseAudioQt::Sink *sink)
{
    if (!sink || !sink->isDefault()) {
        qWarning() << "Audio: received invalid sink";
        return;
    }
    
    if (m_sink) {
        disconnect(m_sink, nullptr, this, nullptr);
    }

    m_sink = sink;

    if (!m_sink) {
        qWarning() << "Audio: Default sink missing";
        return;
    }

    connect(m_sink, &PulseAudioQt::VolumeObject::volumeChanged,
            this, &Audio::onVolumeChanged);
    onVolumeChanged();
}

void Audio::onVolumeChanged()
{
    if (!m_sink) return;

    int percent = paToPercent(m_sink->volume());
    if (percent ==  m_sinkVolume->getValue()) return;

    m_sinkVolume->setValue(percent);

    qDebug() << "Audio: Updated volume from system: " << percent << "%";
}

void Audio::setVolume(int v)
{
    if (!m_sink) return;
    if (v == m_sinkVolume->getValue()) return;

    qint64 paVol = percentToPa(v);
    m_sink->setVolume(paVol);

    qDebug() << "Audio: Set volume to" << v << "%";
}

int Audio::paToPercent(qint64 v) const
{
    double p = (double)v / PulseAudioQt::normalVolume() * 100.0;
    return qRound(p);
}

qint64 Audio::percentToPa(int percent) const
{
    return (qint64)(PulseAudioQt::normalVolume() * (percent / 100.0));
}

void Volume()
{
    new Audio(qApp);
}

REGISTER_INTEGRATION(Volume)
#include "audio.moc"
