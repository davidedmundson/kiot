
#pragma once

#include <map>
#include <mutex>
#include <string>

namespace lva::tr {

struct DeviceInfo {
    std::string device_model;
    std::string model_id;
    std::string firmware_version;
    std::string mac_address;       // verbatim from JSON, may be "AA:BB:..." or "AABB..."
    std::string name;              // configured device name
    std::string status;            // wifi status: "connected" / "connected_no_internet" / ...
    std::string ssid;
    std::string ip;

    // Convenience: MAC stripped to 12-char uppercase hex (AABBCCDDEEFF).
    std::string normalized_mac() const;
    // Convenience: serial == normalized_mac (matches Python convention).
    const std::string& serial_number() const { return mac_; }
    DeviceInfo();

   private:
    friend DeviceInfo ReadDeviceInfo();
    std::string mac_;  // cached normalized form
};

// Reads /data/conf/device.json (default path). Cached by mtime.
DeviceInfo ReadDeviceInfo();

std::string PreferredDeviceName();

}  // namespace lva::tr
