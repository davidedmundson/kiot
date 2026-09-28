#include "tr/Supervisor.h"

#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>

#include "util/Log.h"

extern char** environ;

namespace lva::tr {

namespace {

constexpr const char* kTag = "supervisor";

std::string TimestampNow() {
    char buf[24];
    std::time_t t = std::time(nullptr);
    std::tm tm;
    ::localtime_r(&t, &tm);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}

std::string MakeOtaId() {
    char buf[40];
    std::time_t t = std::time(nullptr);
    std::tm tm;
    ::localtime_r(&t, &tm);
    char ts[16];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm);

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned> dist(0, 0xFFFFFFFFu);
    std::snprintf(buf, sizeof(buf), "ota_%s_%08x", ts, dist(gen));
    return std::string(buf);
}

}  // namespace

Supervisor::Supervisor() {
    state_.status  = "idle";
    state_.message = "No OTA in progress";
    CleanupStaleArtifacts();
}

Supervisor::~Supervisor() {
    cancel_.store(true, std::memory_order_relaxed);
    if (worker_.joinable()) worker_.join();
}

OtaState Supervisor::GetOtaState() const {
    std::lock_guard<std::mutex> lk(state_mtx_);
    return state_;
}

bool Supervisor::IsOtaRunning() const {
    return ota_running_.load(std::memory_order_relaxed);
}

void Supervisor::UpdateState(const OtaState& s) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    state_ = s;
}

void Supervisor::SetStatus(const std::string& status,
                           int progress,
                           const std::string& message) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    state_.status   = status;
    state_.progress = progress;
    state_.message  = message;
}

void Supervisor::MarkFailed(const std::string& ota_id,
                            const std::string& error) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    state_.ota_id      = ota_id;
    state_.status      = "failed";
    state_.progress    = 100;
    state_.finish_time = TimestampNow();
    state_.message     = error;
    LVA_LOGW(kTag, "OTA failed: %s", error.c_str());
}

void Supervisor::CleanupStaleArtifacts() {
    static const char* kStale[] = {
        OtaClient::kDefaultDownloadPath,
        "/data/software.swu.part",
        "/data/conf/ota_history.json",
        "/data/conf/ota_history.json.tmp",
        nullptr,
    };
    for (int i = 0; kStale[i] != nullptr; ++i) {
        if (::unlink(kStale[i]) == 0) {
            LVA_LOGI(kTag, "removed stale OTA artifact: %s", kStale[i]);
        }
    }
}

std::string Supervisor::StartOtaUpdateAsync(const OtaRelease& release) {
    bool expected = false;
    if (!ota_running_.compare_exchange_strong(expected, true,
                                              std::memory_order_acq_rel)) {
        LVA_LOGW(kTag, "OTA already running; rejecting new request");
        return "";
    }
    if (worker_.joinable()) worker_.join();  // join previous

    cancel_.store(false, std::memory_order_relaxed);
    const std::string ota_id = MakeOtaId();
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        state_.ota_id      = ota_id;
        state_.status      = "download";
        state_.progress    = 0;
        state_.start_time  = TimestampNow();
        state_.finish_time = "";
        state_.message     = "Starting OTA update...";
    }
    worker_ = std::thread([this, ota_id, release] {
        OtaWorker(ota_id, release);
    });
    return ota_id;
}

void Supervisor::OtaWorker(std::string ota_id, OtaRelease release) {
    LVA_LOGI(kTag, "OTA worker start id=%s version=%s",
             ota_id.c_str(), release.version.c_str());
    LVA_LOGI(kTag, "OTA url=%s", release.url.c_str());
    try {
        ota_client_.DownloadFirmware(
            release,
            [this](float pct, bool has_progress) {
                std::lock_guard<std::mutex> lk(state_mtx_);
                state_.status   = "download";
                state_.progress = static_cast<int>(pct);
                if (has_progress) {
                    char buf[80];
                    std::snprintf(buf, sizeof(buf),
                                  "Downloading firmware... %.1f%%", pct);
                    state_.message = buf;
                } else {
                    state_.message = "Downloading firmware...";
                }
            },
            &cancel_);

        SetStatus("install", 100, "Starting installation...");
        ota_client_.InstallFirmware();
        {
            std::lock_guard<std::mutex> lk(state_mtx_);
            state_.status      = "install";
            state_.progress    = 100;
            state_.finish_time = TimestampNow();
            state_.message     = "Installation complete; awaiting reboot";
        }
    } catch (const std::exception& e) {
        MarkFailed(ota_id, e.what());
    } catch (...) {
        MarkFailed(ota_id, "OTA failed (unknown error)");
    }
    ota_running_.store(false, std::memory_order_release);
}

void Supervisor::PerformReboot() {
    // Sync 3x to flush NAND. Best-effort.
    for (int i = 0; i < 3; ++i) {
        std::system("sync");
    }
    LVA_LOGI(kTag, "performing reboot");
    char prog[]  = "/sbin/reboot";
    char* argv[] = { prog, nullptr };
    pid_t pid = 0;
    if (::posix_spawn(&pid, prog, nullptr, nullptr,
                      argv, environ) != 0) {
        // Fallback: try /usr/bin/reboot
        char prog2[]  = "/usr/bin/reboot";
        char* argv2[] = { prog2, nullptr };
        ::posix_spawn(&pid, prog2, nullptr, nullptr, argv2, environ);
    }
}

void Supervisor::PerformFactoryReset() {
    for (int i = 0; i < 3; ++i) {
        std::system("sync");
    }
    LVA_LOGI(kTag, "performing factory reset");
    char prog[]  = "/etc/adckey/adckey_function.sh";
    char arg1[]  = "longpressHome";
    char* argv[] = { prog, arg1, nullptr };
    pid_t pid = 0;
    if (::posix_spawn(&pid, prog, nullptr, nullptr, argv, environ) != 0) {
        LVA_LOGE(kTag, "factory reset spawn failed: %s",
                 std::strerror(errno));
    }
}

}  // namespace lva::tr
