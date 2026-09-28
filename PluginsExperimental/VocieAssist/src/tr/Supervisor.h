
#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "tr/OtaClient.h"

namespace lva::tr {

struct OtaState {
    std::string ota_id;       // unique id assigned at start
    std::string status;       // "idle" / "download" / "install" / "failed" / "success"
    int         progress = 0; // 0..100
    std::string start_time;   // ISO-ish "YYYY-MM-DD HH:MM:SS"
    std::string finish_time;
    std::string message;      // human-readable
};

class Supervisor {
   public:
    Supervisor();
    ~Supervisor();

    Supervisor(const Supervisor&)            = delete;
    Supervisor& operator=(const Supervisor&) = delete;

    // Threadsafe.
    OtaState GetOtaState() const;

    std::string StartOtaUpdateAsync(const OtaRelease& release);

    // True iff an OTA is currently in progress.
    bool IsOtaRunning() const;

    // Synchronous helpers used by the HTTP server.
    static void PerformReboot();
    static void PerformFactoryReset();

   private:
    void OtaWorker(std::string ota_id, OtaRelease release);
    void UpdateState(const OtaState& s);
    void SetStatus(const std::string& status,
                   int progress,
                   const std::string& message);
    void MarkFailed(const std::string& ota_id,
                    const std::string& error);
    void CleanupStaleArtifacts();

    OtaClient                 ota_client_;
    mutable std::mutex        state_mtx_;
    OtaState                  state_;
    std::atomic<bool>         ota_running_{false};
    std::atomic<bool>         cancel_{false};
    std::thread               worker_;
};

}  // namespace lva::tr
