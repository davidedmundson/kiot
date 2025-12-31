// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

// SPDX-FileCopyrightText: 1998 Sven Radej <sven@lisa.exp.univie.ac.at>
//      SPDX-FileCopyrightText: 2006 Dirk Mueller <mueller@kde.org>
//          SPDX-FileCopyrightText: 2007 Flavio Castelli <flavio.castelli@gmail.com>

#include "core.h"
#include "entities/binarysensor.h"
#include <KIdleTime>

#include <QDir>
#include <QSocketNotifier>
#include <QTimer>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(cam)
Q_LOGGING_CATEGORY(cam, "integration.Camera")

class CameraWatcher : public QObject
{
    Q_OBJECT
public:
    CameraWatcher(QObject *parent);
    ~CameraWatcher();

private:
    BinarySensor *m_sensor;
    void onInotifyCallback();
    void onInotifyEvent(const inotify_event *event);
    void onVideoDeviceAdded(const QString &devicePath);
    void onVideoDeviceRemoved(const QString &devicePath);

    int m_inotifyFd = -1;
    QSocketNotifier *m_notifier = nullptr;
    QHash<QString, int> m_watchFds;
    QHash<QString, int> m_deviceOpenCounts;
    QTimer *m_hysterisisDelay = nullptr;

    void updateSensorState();
};

CameraWatcher::CameraWatcher(QObject *parent)
    : QObject(parent)
    , m_hysterisisDelay(new QTimer(this))
{
    m_sensor = new BinarySensor(this);
    m_sensor->setId("camera");
    m_sensor->setName("Camera Active");
    m_sensor->setDiscoveryConfig("icon", "mdi:camera");
    m_sensor->setState(false);

    m_hysterisisDelay->setInterval(1000);
    m_hysterisisDelay->setSingleShot(true);
    connect(m_hysterisisDelay, &QTimer::timeout, this, [this]() {
        m_sensor->setState(true);
    });

    m_inotifyFd = inotify_init();
    (void)fcntl(m_inotifyFd, F_SETFD, FD_CLOEXEC);
    inotify_add_watch(m_inotifyFd, "/dev", IN_CREATE | IN_DELETE);

    QDir devDir("/dev");
    devDir.setFilter(QDir::System);
    devDir.setNameFilters({"video*"});
    for (const QString &entry : devDir.entryList()) {
        onVideoDeviceAdded("/dev/" + entry);
    }

    m_notifier = new QSocketNotifier(m_inotifyFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &CameraWatcher::onInotifyCallback);
    m_notifier->setEnabled(true);
}

CameraWatcher::~CameraWatcher()
{
    for (int fd : m_watchFds.values()) {
        if (fd != -1) {
            inotify_rm_watch(m_inotifyFd, fd);
        }
    }
    if (m_inotifyFd != -1) {
        close(m_inotifyFd);
    }
}

void CameraWatcher::onInotifyCallback()
{
    int pending = -1;
    int offsetStartRead = 0; // where we read into buffer
    char buf[8192];
    ioctl(m_inotifyFd, FIONREAD, &pending);

    // copied from KDirWatchPrivate::processInotifyEvents
    while (pending > 0) {
        const int bytesToRead = qMin<int>(pending, sizeof(buf) - offsetStartRead);

        int bytesAvailable = read(m_inotifyFd, &buf[offsetStartRead], bytesToRead);
        pending -= bytesAvailable;
        bytesAvailable += offsetStartRead;
        offsetStartRead = 0;

        int offsetCurrent = 0;
        while (bytesAvailable >= int(sizeof(struct inotify_event))) {
            const struct inotify_event *const event = reinterpret_cast<inotify_event *>(&buf[offsetCurrent]);

            const int eventSize = sizeof(struct inotify_event) + event->len;
            if (bytesAvailable < eventSize) {
                break;
            }

            bytesAvailable -= eventSize;
            offsetCurrent += eventSize;

            onInotifyEvent(event);
        }
        if (bytesAvailable > 0) {
            // copy partial event to beginning of buffer
            memmove(buf, &buf[offsetCurrent], bytesAvailable);
            offsetStartRead = bytesAvailable;
        }
    }
}

void CameraWatcher::onInotifyEvent(const struct inotify_event *event)
{
    QString deviceName = QString::fromLatin1(event->name);
    if (event->mask & IN_CREATE) {
        if (deviceName.startsWith("video")) {
            QTimer::singleShot(5000, this, [this, deviceName]() {
                onVideoDeviceAdded("/dev/" + deviceName);
            });
        }
    }

    if (event->mask & IN_DELETE) {
        if (deviceName.startsWith("video")) {
            onVideoDeviceRemoved("/dev/" + deviceName);
        }
    }

    if (event->mask & (IN_OPEN | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE | IN_DELETE_SELF)) {
        QString devPath;
        for (auto it = m_watchFds.constBegin(); it != m_watchFds.constEnd(); ++it) {
            if (it.value() == event->wd) {
                devPath = it.key();
                break;
            }
        }
        if (devPath.isEmpty())
            return;

        if (event->mask & IN_OPEN) {
            m_deviceOpenCounts[devPath]++;
        } else if (event->mask & (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE)) {
            if (m_deviceOpenCounts.contains(devPath) && m_deviceOpenCounts[devPath] > 0)
                m_deviceOpenCounts[devPath]--;
        } else if (event->mask & IN_DELETE_SELF) {
            m_deviceOpenCounts.remove(devPath);
        }

        updateSensorState();
    }
}

void CameraWatcher::updateSensorState()
{
    int totalOpen = 0;
    for (int val : m_deviceOpenCounts.values())
        totalOpen += val;

    if (totalOpen == 0) {
        m_hysterisisDelay->stop();
        m_sensor->setState(false);
    } else if (totalOpen == 1) {
        m_hysterisisDelay->start();
    }
}

void CameraWatcher::onVideoDeviceAdded(const QString &devicePath)
{
    int wd = inotify_add_watch(m_inotifyFd, devicePath.toUtf8().constData(), IN_OPEN | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE | IN_DELETE_SELF);
    if (wd == -1) {
        qCWarning(cam) << "Failed to watch" << devicePath;
        return;
    }
    m_watchFds[devicePath] = wd;
}

void CameraWatcher::onVideoDeviceRemoved(const QString &devicePath)
{
    int fd = m_watchFds.take(devicePath);
    if (fd >= 1) {
        inotify_rm_watch(m_inotifyFd, fd);
    }
    m_deviceOpenCounts.remove(devicePath);
    updateSensorState();
}

void setupCamera()
{
    new CameraWatcher(qApp);
}

REGISTER_INTEGRATION("CameraWatcher", setupCamera, true)
#include "camera.moc"
