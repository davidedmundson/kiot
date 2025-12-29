// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include "entities/binarysensor.h"
#include <QApplication>
#include <QSocketNotifier>
#include <QTimer>
#include <libudev.h>
#include <unistd.h>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(gamepad)
Q_LOGGING_CATEGORY(gamepad, "integration.Gamepad")

class Gamepad : public QObject
{
    Q_OBJECT
public:
    explicit Gamepad(QObject *parent = nullptr);
    ~Gamepad();

private slots:
    void udevEvent();

private:
    void updateState();
    BinarySensor *m_sensor;
    struct udev *m_udev = nullptr;
    struct udev_monitor *m_monitor = nullptr;
    QSocketNotifier *m_notifier = nullptr;
};

Gamepad::Gamepad(QObject *parent)
    : QObject(parent)
{
    m_sensor = new BinarySensor(this);
    m_sensor->setId("gamepad_connected");
    m_sensor->setName("Gamepad Connected");
    // Sett opp udev
    m_udev = udev_new();
    if (!m_udev) {
        qCWarning(gamepad) << "Failed to create udev context";
        m_sensor->setState(false);
        return;
    }

    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "input", nullptr);
    udev_monitor_enable_receiving(m_monitor);

    int fd = udev_monitor_get_fd(m_monitor);
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &Gamepad::udevEvent);

    // Init state
    updateState();
}
Gamepad::~Gamepad()
{
    if (m_monitor)
        udev_monitor_unref(m_monitor);
    if (m_udev)
        udev_unref(m_udev);
}

void Gamepad::udevEvent()
{
    struct udev_device *dev = udev_monitor_receive_device(m_monitor);
    if (dev) {
        const char *action = udev_device_get_action(dev);
        if (action && (strcmp(action, "add") == 0 || strcmp(action, "remove") == 0)) {
            updateState();
        }
        udev_device_unref(dev);
    }
}

void Gamepad::updateState()
{
    // Sjekk om vi har Gamepadd/joystick enheter
    struct udev_enumerate *enumerate = udev_enumerate_new(m_udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    bool connected = false;
    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, devices)
    {
        const char *path = udev_list_entry_get_name(entry);
        struct udev_device *dev = udev_device_new_from_syspath(m_udev, path);
        if (dev) {
            const char *name = udev_device_get_sysname(dev);
            if (name && strstr(name, "js") != nullptr) { // enkle js* devices
                connected = true;
                udev_device_unref(dev);
                break;
            }
            udev_device_unref(dev);
        }
    }
    udev_enumerate_unref(enumerate);
    m_sensor->setState(connected);
}
void setupGamepad()
{
    new Gamepad(qApp);
}

REGISTER_INTEGRATION("Gamepad", setupGamepad, true)

#include "gamepad.moc"
