#include "tr/TimeZoneSync.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "util/Log.h"

namespace lva::tr {

namespace {

constexpr const char* kTag           = "tz";
constexpr const char* kZoneInfoDir   = "/usr/share/zoneinfo/";
constexpr const char* kEtcLocaltime  = "/etc/localtime";
constexpr const char* kEtcTimezone   = "/etc/timezone";

bool IsSafeIanaName(const std::string& s) {
    if (s.empty()) return false;
    if (s.size() > 64) return false;
    if (s.front() == '/' || s.find("..") != std::string::npos) {
        return false;
    }
    for (char c : s) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) ||
              c == '_' || c == '/' || c == '-' || c == '+')) {
            return false;
        }
    }
    return true;
}

const char* PosixTzToIana(const std::string& tz) {
    struct E { const char* posix; const char* iana; };
    static const E kMap[] = {
        // China / Hong Kong / Taiwan
        {"CST-8",    "Asia/Shanghai"},
        {"HKT-8",    "Asia/Hong_Kong"},
        {"PHT-8",    "Asia/Manila"},
        {"SGT-8",    "Asia/Singapore"},
        // Japan / Korea
        {"JST-9",    "Asia/Tokyo"},
        {"KST-9",    "Asia/Seoul"},
        // India
        {"IST-5:30", "Asia/Kolkata"},
        {"GMT0",     "Europe/London"},
        {"GMT0BST,M3.5.0/1,M10.5.0", "Europe/London"},
        {"CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Berlin"},
        {"EET-2EEST,M3.5.0/3,M10.5.0/4", "Europe/Helsinki"},
        {"EST5EDT,M3.2.0,M11.1.0", "America/New_York"},
        {"CST6CDT,M3.2.0,M11.1.0", "America/Chicago"},
        {"MST7MDT,M3.2.0,M11.1.0", "America/Denver"},
        {"PST8PDT,M3.2.0,M11.1.0", "America/Los_Angeles"},
    };
    for (const auto& e : kMap) {
        if (tz == e.posix) return e.iana;
    }
    return nullptr;
}

std::string ReadEtcTimezone() {
    std::ifstream f(kEtcTimezone);
    if (!f.is_open()) return {};
    std::string s;
    std::getline(f, s);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' ||
                          s.back() == ' ')) {
        s.pop_back();
    }
    return s;
}

}  // namespace

bool ApplyTimezone(const std::string& zone_arg) {
    std::string iana_name = zone_arg;

    if (const char* mapped = PosixTzToIana(zone_arg)) {
        iana_name = mapped;
        LVA_LOGI(kTag, "translated POSIX '%s' -> IANA '%s'",
                 zone_arg.c_str(), iana_name.c_str());
    }

    if (!IsSafeIanaName(iana_name)) {
        LVA_LOGW(kTag, "rejecting unsafe timezone string: %s",
                 iana_name.c_str());
        return false;
    }

    // No-op if already set.
    if (ReadEtcTimezone() == iana_name) {
        return true;
    }

    const std::string zone_path = std::string(kZoneInfoDir) + iana_name;
    struct stat st{};
    if (::stat(zone_path.c_str(), &st) != 0) {
        LVA_LOGW(kTag, "zoneinfo missing for %s: %s; "
                       "applying TZ=%s to this process only",
                 iana_name.c_str(), std::strerror(errno),
                 zone_arg.c_str());
        ::setenv("TZ", zone_arg.c_str(), 1);
        ::tzset();
        return false;
    }

    const std::string tmp_path = std::string(kEtcLocaltime) + ".tmp";
    ::unlink(tmp_path.c_str());  // best-effort
    if (::symlink(zone_path.c_str(), tmp_path.c_str()) != 0) {
        LVA_LOGW(kTag, "symlink(%s -> %s) failed: %s",
                 tmp_path.c_str(), zone_path.c_str(),
                 std::strerror(errno));
        return false;
    }
    if (::rename(tmp_path.c_str(), kEtcLocaltime) != 0) {
        ::unlink(tmp_path.c_str());
        LVA_LOGW(kTag, "rename(%s -> %s) failed: %s",
                 tmp_path.c_str(), kEtcLocaltime,
                 std::strerror(errno));
        return false;
    }

    // Mirror /etc/timezone (text file).
    {
        const std::string tz_tmp = std::string(kEtcTimezone) + ".tmp";
        std::ofstream f(tz_tmp);
        if (f.is_open()) {
            f << iana_name << "\n";
            f.close();
            if (::rename(tz_tmp.c_str(), kEtcTimezone) != 0) {
                ::unlink(tz_tmp.c_str());
                LVA_LOGW(kTag,
                         "rename(/etc/timezone) failed: %s",
                         std::strerror(errno));
            }
        } else {
            LVA_LOGW(kTag, "cannot write %s", kEtcTimezone);
        }
    }

    ::setenv("TZ", iana_name.c_str(), 1);
    ::tzset();

    LVA_LOGI(kTag, "updated to %s", iana_name.c_str());
    return true;
}

}  // namespace lva::tr
