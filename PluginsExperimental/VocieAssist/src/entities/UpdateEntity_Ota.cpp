
#include "entities/UpdateEntity.h"

#include <chrono>
#include <thread>
#include <stdexcept>
#include <utility>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "state/ServerState.h"
#include "tr/OtaClient.h"
#include "tr/Supervisor.h"
#include "tr/SysInfo.h"
#include "util/Log.h"

namespace lva::entities {

namespace {

constexpr const char* kTag = "update";

constexpr const char* kCheckVersionUrl =
    "https://ota.cloud.3reality.com/reality/ota/client/checkVersion";
constexpr const char* kCaCertPath = "/etc/ssl/certs/ca-certificates.crt";
constexpr const char* kReleaseUrlBase =
    "https://github.com/thirdreality/voice-music-assistant/releases";
constexpr const char* kReleaseTagBase =
    "https://github.com/thirdreality/voice-music-assistant/releases/tag/";

std::string CloudVersionToTag(const std::string& v) {
    std::string out;
    out.reserve(v.size());
    std::size_t start = 0;
    for (std::size_t i = 0; i <= v.size(); ++i) {
        if (i == v.size() || v[i] == '.') {
            std::size_t s = start;
            while (s + 1 < i && v[s] == '0') ++s;
            out.append(v, s, i - s);
            if (i < v.size()) out.push_back('.');
            start = i + 1;
        }
    }
    return out;
}

std::size_t WriteToString(char* ptr, std::size_t size, std::size_t nmemb,
                          void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    const std::size_t n = size * nmemb;
    constexpr std::size_t kMaxCheckResponse = 64 * 1024;
    if (s->size() + n > kMaxCheckResponse) {
        return 0;  // signals error to curl
    }
    s->append(ptr, n);
    return n;
}

std::string CheckVersionHttp(const std::string& body) {
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        throw std::runtime_error("curl_easy_init failed");
    }
    std::string response;
    struct curl_slist* hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,            kCheckVersionUrl);
    curl_easy_setopt(curl, CURLOPT_POST,           1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     hdrs);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                     static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  &WriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        20L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_CAINFO,         kCaCertPath);

    const CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        throw std::runtime_error(std::string("curl: ") +
                                 curl_easy_strerror(rc));
    }
    if (http_code != 200) {
        throw std::runtime_error("HTTP " + std::to_string(http_code));
    }
    return response;
}

}  // namespace

void UpdateEntity::ScheduleCheck() {
    bool expected = false;
    if (!worker_busy_.compare_exchange_strong(expected, true)) {
        LVA_LOGW(kTag, "OTA check already running");
        return;
    }
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this] {
        try {
            CheckOnce();
        } catch (const std::exception& e) {
            LVA_LOGW(kTag, "check_version failed: %s", e.what());
        }
        worker_busy_.store(false, std::memory_order_release);
    });
}

void UpdateEntity::ScheduleInstall() {
    if (state_.supervisor == nullptr) {
        LVA_LOGW(kTag, "no supervisor — can't install");
        return;
    }
    if (download_url_.empty() || expected_md5_.empty()) {
        LVA_LOGW(kTag, "no cached release info — running CHECK first");
        ScheduleCheck();
        return;
    }

    bool expected = false;
    if (!worker_busy_.compare_exchange_strong(expected, true)) {
        LVA_LOGW(kTag, "OTA worker already busy");
        return;
    }
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this] {
        try {
            InstallOnce();
        } catch (const std::exception& e) {
            LVA_LOGW(kTag, "install failed: %s", e.what());
            in_progress_     = false;
            release_summary_ = std::string("Install failed: ") + e.what();
            BroadcastState();
        }
        worker_busy_.store(false, std::memory_order_release);
    });
}

void UpdateEntity::CheckOnce() {
    using nlohmann::json;
    const lva::tr::DeviceInfo dev = lva::tr::ReadDeviceInfo();
    if (dev.model_id.empty() || dev.serial_number().empty()) {
        LVA_LOGW(kTag, "device.json missing modelID/macAddress");
        return;
    }
    current_version_ = state_.version;

    json req = {
        {"modelId", dev.model_id},
        {"version", current_version_},
        {"sno",     dev.serial_number()},
    };
    const std::string req_body = req.dump();
    LVA_LOGI(kTag, "checking OTA version model=%s ver=%s sno=%s",
             dev.model_id.c_str(), current_version_.c_str(),
             dev.serial_number().c_str());
    LVA_LOGD(kTag, "POST %s body=%s",
             kCheckVersionUrl, req_body.c_str());

    std::string body;
    try {
        body = CheckVersionHttp(req_body);
    } catch (const std::exception& e) {
        LVA_LOGW(kTag, "OTA check HTTP failed: %s", e.what());
        return;
    }
    LVA_LOGD(kTag, "response: %s", body.c_str());

    json resp = json::parse(body, nullptr, false);
    if (resp.is_discarded() || !resp.is_object()) {
        LVA_LOGW(kTag, "OTA response not valid JSON");
        return;
    }
    if (!resp.contains("result") || !resp["result"].is_object()) {
        LVA_LOGW(kTag, "OTA response missing result");
        return;
    }
    const auto& result = resp["result"];
    const auto& data   = result.value("data", json::object());

    bool has_update = data.value("hasNewVersion", false);
    std::string latest = data.value("displayVersion", std::string());
    if (latest.empty()) latest = data.value("version", std::string());
    if (latest.empty()) latest = current_version_;

    expected_md5_ = data.value("md5", std::string());
    download_url_ = data.value("binUrl", std::string());
    if (download_url_.empty()) {
        download_url_ = data.value("altBinUrl", std::string());
    }

    latest_version_ = has_update ? latest : current_version_;
    if (has_update) {
        release_summary_ = "New firmware available";
    } else {
        release_summary_ = "";   // empty hides the line in HA's card
    }
    if (has_update && !latest_version_.empty()) {
        release_url_ = std::string(kReleaseTagBase) + "v" +
                       CloudVersionToTag(latest_version_);
    } else {
        release_url_ = kReleaseUrlBase;
    }

    LVA_LOGI(kTag, "current=%s latest=%s has_update=%d",
             current_version_.c_str(), latest_version_.c_str(),
             has_update ? 1 : 0);
    BroadcastState();
}

void UpdateEntity::InstallOnce() {
    in_progress_     = true;
    has_progress_    = true;
    progress_        = 0.0f;
    release_summary_ = "Downloading firmware...";
    BroadcastState();

    lva::tr::OtaRelease release;
    release.url          = download_url_;
    release.version      = latest_version_;
    release.expected_md5 = expected_md5_;

    const std::string ota_id =
        state_.supervisor->StartOtaUpdateAsync(release);
    if (ota_id.empty()) {
        in_progress_     = false;
        release_summary_ = "OTA already in progress";
        BroadcastState();
        return;
    }

    int last_pct = -1;
    while (state_.supervisor->IsOtaRunning()) {
        const auto s = state_.supervisor->GetOtaState();
        if (s.progress != last_pct) {
            progress_        = static_cast<float>(s.progress);
            release_summary_ = s.message;
            BroadcastState();
            last_pct = s.progress;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    const auto final_state = state_.supervisor->GetOtaState();
    progress_        = 100.0f;
    in_progress_     = false;
    release_summary_ = final_state.message;
    if (final_state.status != "failed") {
        current_version_ = latest_version_;
    }
    BroadcastState();
}

}  // namespace lva::entities
