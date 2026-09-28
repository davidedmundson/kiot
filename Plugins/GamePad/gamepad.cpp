

#include "gamepad.h"

DEFINE_PLUGIN_LOGGER(gamepad,Gamepad)
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
    struct udev_enumerate *enumerate = udev_enumerate_new(m_udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    bool connected = false;
    QString devName = "Unknown";
    QString devModel = "Unknown";
    QString devVendor = "Unknown";
    QString dev_productName = "Unknown";
    QString dev_VendorName = "Unknown";
    QString friendlyControllerName = "Unknown";
    
    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, devices)
    {
        const char *path = udev_list_entry_get_name(entry);
        struct udev_device *dev = udev_device_new_from_syspath(m_udev, path);
        if (dev) {
            const char *name = udev_device_get_sysname(dev);
            
            // Vi sjekker om det er en joystick-enhet
            if (name && strstr(name, "js") != nullptr) {
                connected = true;
                devName = QString::fromUtf8(name);

                struct udev_device *parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
                
                if (!parent) {
                    parent = udev_device_get_parent_with_subsystem_devtype(dev, "bluetooth", nullptr);
                }

                if (parent) {
                    const char *vendor = udev_device_get_sysattr_value(parent, "idVendor");
                    const char *model = udev_device_get_sysattr_value(parent, "idProduct"); // Eller "product" for tekst
                    const char *model_db = udev_device_get_property_value(parent, "ID_MODEL_FROM_DATABASE");
                    const char *product_name = udev_device_get_sysattr_value(parent, "product");
                    const char *vendor_name = udev_device_get_sysattr_value(parent, "manufacturer");
                    const char *friendlyName = udev_device_get_sysattr_value(dev, "ID_MODEL_ENC");
                    friendlyControllerName = friendlyName ? QString::fromUtf8(friendlyName) : "Unknown";
                    dev_productName = product_name ? QString::fromUtf8(product_name) : "Unknown";
                    dev_VendorName = vendor_name ? QString::fromUtf8(vendor_name) : "Unknown";
                    devVendor = vendor ? QString::fromUtf8(vendor) : "Unknown";
                    // Prioriter DB-navn hvis det finnes, ellers bruk Hex-ID
                    devModel = model_db ? QString::fromUtf8(model_db) : (model ? QString::fromUtf8(model) : "Unknown");
                }
                
                udev_device_unref(dev);
                break; // Vi fant en, stopper her for nå
            }
            udev_device_unref(dev);
        }
    }
    udev_enumerate_unref(enumerate);
    m_sensor->setAttributes({
        {"friendlyControllerName", friendlyControllerName},
        {"devName", devName},
        {"devModel", devModel},
        {"productName", dev_productName},
        {"devVendor", devVendor},
        {"VendorName", dev_VendorName}

    });
    m_sensor->setState(connected);
    
}