#include "tr/SysInfo.h"

#include <sys/stat.h>

#include <cctype>
#include <fstream>
#include <mutex>
#include <sstream>

#include <nlohmann/json.hpp>

#include "util/Log.h"

namespace lva::tr {

namespace {

constexpr const char* kTag        = "sysinfo";
constexpr const char* kDevicePath = "/data/conf/device.json";

std::mutex             g_cache_mtx;
DeviceInfo             g_cache;
struct timespec        g_cache_mtime{0, 0};

std::string Strip(const std::string& s) {
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

}  // namespace

DeviceInfo::DeviceInfo() = default;

std::string DeviceInfo::normalized_mac() const {
    if (!mac_.empty()) return mac_;
    std::string out;
    for (char c : mac_address) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        if (out.size() == 12) break;
    }
    return out;
}

DeviceInfo ReadDeviceInfo() {
    struct stat st{};
    if (::stat(kDevicePath, &st) != 0) {
        // No file — return a default-constructed (empty) struct.
        std::lock_guard<std::mutex> lk(g_cache_mtx);
        g_cache_mtime = {0, 0};
        g_cache       = DeviceInfo();
        return g_cache;
    }

    {
        std::lock_guard<std::mutex> lk(g_cache_mtx);
        if (st.st_mtim.tv_sec  == g_cache_mtime.tv_sec &&
            st.st_mtim.tv_nsec == g_cache_mtime.tv_nsec) {
            return g_cache;
        }
    }

    DeviceInfo info;
    try {
        std::ifstream f(kDevicePath);
        if (!f.is_open()) {
            LVA_LOGW(kTag, "open(%s) failed", kDevicePath);
        } else {
            nlohmann::json data = nlohmann::json::parse(f, nullptr, false);
            if (!data.is_discarded() && data.is_object()) {
                if (data.contains("device") && data["device"].is_object()) {
                    const auto& d = data["device"];
                    info.device_model     = Strip(d.value("deviceModel", ""));
                    info.model_id         = Strip(d.value("modelID", ""));
                    info.firmware_version = Strip(d.value("firmwareVersion", ""));
                    info.mac_address      = Strip(d.value("macAddress", ""));
                    info.name             = Strip(d.value("name", ""));
                }
                if (data.contains("network") && data["network"].is_object()) {
                    const auto& n = data["network"];
                    info.status = Strip(n.value("status", ""));
                    info.ssid   = Strip(n.value("ssid", ""));
                    info.ip     = Strip(n.value("ip", ""));
                }
            } else {
                LVA_LOGW(kTag, "%s is not valid JSON object", kDevicePath);
            }
        }
    } catch (const std::exception& e) {
        LVA_LOGW(kTag, "%s parse error: %s", kDevicePath, e.what());
    }

    info.mac_ = info.normalized_mac();

    {
        std::lock_guard<std::mutex> lk(g_cache_mtx);
        g_cache_mtime = st.st_mtim;
        g_cache       = info;
    }
    return info;
}

std::string PreferredDeviceName() {
    DeviceInfo info = ReadDeviceInfo();
    if (!info.name.empty()) return info.name;
    if (!info.serial_number().empty()) {
        return std::string("3RSPK-") + info.serial_number();
    }
    return "3RSPK-DEFAULT";
}

}  // namespace lva::tr
