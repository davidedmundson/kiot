
#include "entities/UpdateEntity.h"

#include <ctime>
#include <functional>

#include "protocol/MessageRegistry.h"
#include "state/ServerState.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::entities {

namespace {

constexpr const char* kTag         = "update";
constexpr const char* kObjectId    = "thirdreality_firmware_update";
constexpr const char* kDisplayName = "Firmware Update";
constexpr const char* kTitle       = "ThirdReality Firmware";
constexpr const char* kReleaseUrl  =
    "https://github.com/thirdreality/voice-music-assistant/releases";

constexpr int kEnumUpdateCmdNone    = 0;
constexpr int kEnumUpdateCmdCheck   = 2;
constexpr int kEnumUpdateCmdInstall = 1;

}  // namespace

UpdateEntity::UpdateEntity(std::uint32_t key, lva::state::ServerState& state)
    : Entity(key, kObjectId, kDisplayName), state_(state) {}

void UpdateEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesUpdateResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    resp.set_entity_category(::ENTITY_CATEGORY_CONFIG);
    resp.set_device_class("firmware");
    resp.set_icon("mdi:update");
    sink(lva::proto::kIdListEntitiesUpdateResponse, resp);
}

void UpdateEntity::EmitState(const ResponseSink& sink) const {
    ::UpdateStateResponse resp;
    resp.set_key(key());
    resp.set_missing_state(false);
    resp.set_in_progress(in_progress_);
    resp.set_has_progress(has_progress_);
    resp.set_progress(progress_);
    resp.set_current_version(current_version_.empty()
                                 ? state_.version : current_version_);
    resp.set_latest_version(latest_version_.empty()
                                ? state_.version : latest_version_);
    resp.set_title(kTitle);
    resp.set_release_summary(release_summary_);
    resp.set_release_url(release_url_.empty() ? kReleaseUrl : release_url_);
    sink(lva::proto::kIdUpdateStateResponse, resp);
}

void UpdateEntity::OnSubscribeStates(const ResponseSink& sink) {
    EmitState(sink);
}

void UpdateEntity::BroadcastState() {
    if (!state_.broadcast) return;
    ::UpdateStateResponse resp;
    resp.set_key(key());
    resp.set_missing_state(false);
    resp.set_in_progress(in_progress_);
    resp.set_has_progress(has_progress_);
    resp.set_progress(progress_);
    resp.set_current_version(current_version_.empty()
                                 ? state_.version : current_version_);
    resp.set_latest_version(latest_version_.empty()
                                ? state_.version : latest_version_);
    resp.set_title(kTitle);
    resp.set_release_summary(release_summary_);
    resp.set_release_url(release_url_.empty() ? kReleaseUrl : release_url_);
    state_.broadcast(lva::proto::kIdUpdateStateResponse, resp);
}

void UpdateEntity::OnCommand(const ::google::protobuf::MessageLite& request,
                             std::uint32_t request_msg_type_id,
                             const ResponseSink& sink) {
    if (request_msg_type_id != lva::proto::kIdUpdateCommandRequest) return;
    const auto& cmd = static_cast<const ::UpdateCommandRequest&>(request);
    if (cmd.key() != key()) return;

    const int command = static_cast<int>(cmd.command());
    LVA_LOGI(kTag, "OnCommand: command=%d", command);

    if (command == kEnumUpdateCmdCheck || command == kEnumUpdateCmdNone) {
        ScheduleCheck();
    } else if (command == kEnumUpdateCmdInstall) {
        ScheduleInstall();
    }
    EmitState(sink);
}

void UpdateEntity::OnPeriodicTick() {
    // Compute device-specific offset once (deterministic from name).
    if (daily_check_offset_min_ < 0) {
        const std::size_t h = std::hash<std::string>{}(state_.name);
        daily_check_offset_min_ = static_cast<int>(h % 60);
        LVA_LOGD(kTag, "daily check offset: %d min past 02:00",
                 daily_check_offset_min_);
    }

    const std::time_t now = std::time(nullptr);
    struct std::tm tm{};
    if (::localtime_r(&now, &tm) == nullptr) return;

    // Already checked today?
    if (tm.tm_yday == last_check_yday_) return;

    // Target: 02:00 + offset
    const int target_min = 2 * 60 + daily_check_offset_min_;
    const int now_min    = tm.tm_hour * 60 + tm.tm_min;
    if (now_min < target_min) return;

    last_check_yday_ = tm.tm_yday;
    LVA_LOGI(kTag, "daily scheduled OTA check (02:%02d)",
             daily_check_offset_min_);
    ScheduleCheck();
}

}  // namespace lva::entities
