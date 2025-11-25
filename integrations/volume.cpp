#include "core.h"

#include <QCoreApplication>
#include <QObject>
#include <QDebug>

#include <KF6/KF6PulseAudioQt/PulseAudioQt/Context>
#include <KF6/KF6PulseAudioQt/PulseAudioQt/Server>
#include <KF6/KF6PulseAudioQt/PulseAudioQt/Sink>
#include <KF6/KF6PulseAudioQt/PulseAudioQt/VolumeObject>

class VolumeWatcher : public QObject
{
    Q_OBJECT

public:
    explicit VolumeWatcher(QObject *parent = nullptr);

private slots:
    void onDefaultSinkChanged(PulseAudioQt::Sink *sink);
    void onVolumeChanged();
    void setVolume(int v);

private:
    int paToPercent(qint64 v) const;
    qint64 percentToPa(int percent) const;

    Number *m_sensor = nullptr;
    PulseAudioQt::Server *m_server = nullptr;
    PulseAudioQt::Sink *m_sink = nullptr;

    int m_currentVolume = 0;
};

// Constructor
VolumeWatcher::VolumeWatcher(QObject *parent)
    : QObject(parent)
{
    m_sensor = new Number(this);
    m_sensor->setId("volume");
    m_sensor->setName("System Volume");
    m_sensor->setRange(0, 100, 1, "%");

    connect(m_sensor, &Number::valueChangeRequested,
            this, &VolumeWatcher::setVolume);

    auto ctx = PulseAudioQt::Context::instance();
    if (!ctx || !ctx->isValid()) {
        qWarning() << "VolumeWatcher: PulseAudio context not valid";
        return;
    }

    m_server = ctx->server();
    if (!m_server) {
        qWarning() << "VolumeWatcher: No PulseAudio server";
        return;
    }

    connect(m_server, &PulseAudioQt::Server::defaultSinkChanged,
            this, &VolumeWatcher::onDefaultSinkChanged);

    PulseAudioQt::Sink *initial = m_server->defaultSink();
    if (initial) {
        onDefaultSinkChanged(initial);
    } else {
        qWarning() << "VolumeWatcher: default sink not ready, waiting for signal";
    }
}

void VolumeWatcher::onDefaultSinkChanged(PulseAudioQt::Sink *sink)
{
    if (!sink || !sink->isDefault()) {
        qWarning() << "VolumeWatcher: received invalid sink";
        return;
    }
    
    if (m_sink) {
        disconnect(m_sink, nullptr, this, nullptr);
    }

    m_sink = sink;

    if (!m_sink) {
        qWarning() << "VolumeWatcher: Default sink missing";
        return;
    }

    connect(m_sink, &PulseAudioQt::VolumeObject::volumeChanged,
            this, &VolumeWatcher::onVolumeChanged);

    int percent = paToPercent(m_sink->volume());
    m_currentVolume = percent;
    m_sensor->setValue(percent);

    qDebug() << "VolumeWatcher: Attached to sink with" << percent << "% volume";
}

void VolumeWatcher::onVolumeChanged()
{
    if (!m_sink) return;

    int percent = paToPercent(m_sink->volume());
    if (percent == m_currentVolume) return;

    m_currentVolume = percent;
    m_sensor->setValue(percent);

    qDebug() << "VolumeWatcher: System volume updated to" << percent << "%";
}

void VolumeWatcher::setVolume(int v)
{
    if (!m_sink) return;
    if (v == m_currentVolume) return;

    qint64 paVol = percentToPa(v);
    m_sink->setVolume(paVol);

    m_currentVolume = v;
    qDebug() << "VolumeWatcher: Set volume to" << v << "%";
}

int VolumeWatcher::paToPercent(qint64 v) const
{
    double p = (double)v / PulseAudioQt::normalVolume() * 100.0;
    return qRound(p);
}

qint64 VolumeWatcher::percentToPa(int percent) const
{
    return (qint64)(PulseAudioQt::normalVolume() * (percent / 100.0));
}

void setupVolume()
{
    new VolumeWatcher(qApp);
}

REGISTER_INTEGRATION(setupVolume)
#include "volume.moc"
