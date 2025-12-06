// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

   // SPDX-FileCopyrightText: 1998 Sven Radej <sven@lisa.exp.univie.ac.at>
   //      SPDX-FileCopyrightText: 2006 Dirk Mueller <mueller@kde.org>
   //          SPDX-FileCopyrightText: 2007 Flavio Castelli <flavio.castelli@gmail.com>

#include "core.h"
#include "entities/entities.h"
#include <KIdleTime>
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QDir>
#include <QTimer>

#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <sys/ioctl.h>

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
    int m_devWatchFd = -1;
    QSocketNotifier *m_notifier;
    QHash<QString, int> m_watchFds;
    int openCount = 0;
    QTimer *m_hysterisisDelay = nullptr;
};

// Note, I don't cover the initial state

CameraWatcher::CameraWatcher(QObject *parent)
    : QObject(parent)
    , m_hysterisisDelay(new QTimer(this))
{
    m_sensor = new BinarySensor(this);
    m_sensor->setId("camera");
    m_sensor->setName("Camera Active");
    m_sensor->setState(false);

    // some apps will query /dev/video0 on startup to see if a webcam is available
    // consider the camera in use if they keep it open for more than a second
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
        qDebug() << "existing video device" << entry;
        onVideoDeviceAdded("/dev/" + entry);
    }

    m_notifier = new QSocketNotifier(m_inotifyFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &CameraWatcher::onInotifyCallback);
    m_notifier->setEnabled(true);
}

CameraWatcher::~CameraWatcher()
{
    if (m_devWatchFd != -1) {
        inotify_rm_watch(m_inotifyFd, m_devWatchFd);
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
    // /dev handling
    if (event->mask & IN_CREATE) {
        QString name = QString::fromLatin1(event->name);
        if (name.startsWith("video")) {
            qDebug() << "about to add video device" << name;
            // the camera seems to need some time to settle down, it's a bit crap
            // maybe we should try to connect multiple times?
            QTimer::singleShot(5000, this, [this, name]() {
                onVideoDeviceAdded("/dev/" + name);
            });
        }
    }
    if (event->mask & IN_DELETE) {
        QString name = QString::fromLatin1(event->name);
        if (name.startsWith("video")) {
            onVideoDeviceRemoved("/dev/" + name);
        }
    }

    // /dev/videoX handling
    if (event->mask & IN_OPEN) {
        openCount++;
    } else if (event->mask & IN_CLOSE_WRITE) {
        openCount--;
    } else if (event->mask & IN_CLOSE_NOWRITE) {
        openCount--;
    }
    // else if (event->mask & IN_DELETE_SELF) { // we probably want this, but it means keeping an open count per device
    //     openCount = 0;
    // }
    Q_ASSERT(openCount >= 0);
    if (openCount == 0) {
        m_hysterisisDelay->stop();
        m_sensor->setState(false);
    } else if (openCount == 1) {
        m_hysterisisDelay->start();
    }
}

void CameraWatcher::onVideoDeviceAdded(const QString &devicePath)
{
    qDebug() << "detected new video device " << devicePath;
    int watchDescriptor = inotify_add_watch(m_inotifyFd, devicePath.toLatin1().constData(), IN_OPEN | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE | IN_DELETE_SELF);
    if (watchDescriptor == -1) {
        qWarning() << "Failed to watch" << devicePath;

        return;
    }
    m_watchFds[devicePath] = watchDescriptor;
}

void CameraWatcher::onVideoDeviceRemoved(const QString &devicePath)
{
    qDebug() << "detected video device removed: " << devicePath;
    int fd = m_watchFds.take(devicePath);
    Q_ASSERT(fd > 0);
    inotify_rm_watch(m_inotifyFd, fd);
}

void setupCamera()
{
    new CameraWatcher(qApp);
}

REGISTER_INTEGRATION("CameraWatcher",setupCamera,true)
#include "camera.moc"
