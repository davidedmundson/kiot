// MdnsPublisher: registers _esphomelib._tcp mDNS service via
// avahi-client so HA can auto-discover this device on the LAN.

#pragma once

#include <cstdint>
#include <string>

namespace lva::proto {

class MdnsPublisher {
   public:
    struct Options {
        std::string name;        // service instance name (device name)
        std::uint16_t port = 6053;
        std::string mac;         // colon-separated MAC
        std::string version;     // esphome_version string
    };

    MdnsPublisher() = default;
    ~MdnsPublisher();

    MdnsPublisher(const MdnsPublisher&) = delete;
    MdnsPublisher& operator=(const MdnsPublisher&) = delete;

    bool Start(const Options& opts);
    void Stop();

   private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace lva::proto
