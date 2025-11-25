//TODO find a way to get the volume changes directly from the OS so we dont need a timer
#include "core.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

class VolumeWatcher : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE VolumeWatcher(QObject *parent);
    ~VolumeWatcher();

private Q_SLOTS:
    void setVolume(int value);
    void updateVolumeFromSystem();

private:
    Number *m_sensor;
    QTimer *m_pollTimer = nullptr;
    int m_currentVolume = 0; // saves last known system volume
};

VolumeWatcher::VolumeWatcher(QObject *parent)
    : QObject(parent)
{
    m_sensor = new Number(this);
    m_sensor->setId("volume");
    m_sensor->setName("System Volume");
    m_sensor->setDiscoveryConfig("icon", "mdi:volume-medium");
    m_sensor->setRange(0, 100, 1, "%");
    
    connect(m_sensor, &Number::valueChangeRequested, this, &VolumeWatcher::setVolume);

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &VolumeWatcher::updateVolumeFromSystem);
    m_pollTimer->start(5000);

    updateVolumeFromSystem();
}

VolumeWatcher::~VolumeWatcher()
{
  
}

void VolumeWatcher::updateVolumeFromSystem()
{
    QProcess *p = new QProcess(this);
    QObject::connect(p, &QProcess::finished, this, [p,  this]() {
        QString out = p->readAllStandardOutput();
        if (out.isEmpty()){
            p->deleteLater();
            return;
        }
        // Example line:
        // Volume: aux0: 58327 / 89% / -3.04 dB, aux1: ...
        QRegularExpression re("(\\d+)%");
        QRegularExpressionMatch match = re.match(out);
        if (!match.hasMatch()) {
            p->deleteLater();
            return;
        }
        int level = match.captured(1).toInt();
        if (level != m_currentVolume) {
            m_currentVolume = level;
            m_sensor->setValue(level);
            qDebug() << "VolumeWatcher updated home assistant to: " << level;
        }
        p->deleteLater();
    });
    p->start("pactl", {"get-sink-volume", "@DEFAULT_SINK@"});
}

void VolumeWatcher::setVolume(int value)
{
    if (value == m_currentVolume) return;

    QProcess *p = new QProcess(this);
    p->start("pactl", {"set-sink-volume", "@DEFAULT_SINK@", QString::number(value) + "%"});
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, p, value](int, QProcess::ExitStatus) {
        m_currentVolume = value;
        m_sensor->setValue(value);
        qDebug() << "VolumeWatcher set volume to" << value;
        p->deleteLater();
    });
}

void setupVolume()
{
    new VolumeWatcher(qApp);
}

REGISTER_INTEGRATION(setupVolume)
#include "volume.moc"
