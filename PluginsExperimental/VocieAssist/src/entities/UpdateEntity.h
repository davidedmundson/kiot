
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "entities/Entity.h"

namespace lva::state { class ServerState; }

namespace lva::entities {

class UpdateEntity final : public Entity {
   public:
    UpdateEntity(std::uint32_t key, lva::state::ServerState& state);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;
    void OnCommand(const ::google::protobuf::MessageLite& request,
                   std::uint32_t request_msg_type_id,
                   const ResponseSink& sink) override;

    // Push current state to all connected HA clients.
    void BroadcastState();

    void OnPeriodicTick();

    // Run a check-version round on the worker thread.
    void TriggerCheck() { ScheduleCheck(); }

   private:
    void EmitState(const ResponseSink& sink) const;
    void ScheduleCheck();
    void ScheduleInstall();
    void CheckOnce();
    void InstallOnce();

    lva::state::ServerState& state_;

    std::string current_version_;
    std::string latest_version_;
    std::string expected_md5_;
    std::string download_url_;
    std::string release_summary_;
    std::string release_url_;

    bool        in_progress_  = false;
    bool        has_progress_ = false;
    float       progress_     = 0.0f;

    std::atomic<bool> worker_busy_{false};
    std::thread       worker_;

    // Daily scheduled check state.
    int  daily_check_offset_min_ = -1;  // [0,60), set on first tick
    int  last_check_yday_        = -1;  // tm_yday of last daily check
};

}  // namespace lva::entities
