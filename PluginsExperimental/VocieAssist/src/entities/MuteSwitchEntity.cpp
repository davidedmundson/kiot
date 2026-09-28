#include "entities/MuteSwitchEntity.h"

#include "protocol/MessageRegistry.h"
#include "state/ServerState.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::entities {

namespace {
constexpr const char* kTag = "mute_sw";
constexpr const char* kObjectId = "mute";
constexpr const char* kDisplayName = "Mute";
}  // namespace

MuteSwitchEntity::MuteSwitchEntity(std::uint32_t key,
                                   lva::state::ServerState& state)
    : Entity(key, kObjectId, kDisplayName), state_(state) {
    state_.muted.store(state_.preferences.is_mic_muted(),
                       std::memory_order_relaxed);
}

void MuteSwitchEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesSwitchResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    resp.set_entity_category(::ENTITY_CATEGORY_CONFIG);
    resp.set_icon("mdi:microphone-off");
    sink(lva::proto::kIdListEntitiesSwitchResponse, resp);
}

void MuteSwitchEntity::OnSubscribeStates(const ResponseSink& sink) {
    ::SwitchStateResponse resp;
    resp.set_key(key());
    resp.set_state(state_.muted.load(std::memory_order_relaxed));
    sink(lva::proto::kIdSwitchStateResponse, resp);
}

void MuteSwitchEntity::OnCommand(
    const ::google::protobuf::MessageLite& request,
    std::uint32_t request_msg_type_id,
    const ResponseSink& sink) {
    if (request_msg_type_id != lva::proto::kIdSwitchCommandRequest) return;
    const auto& cmd = static_cast<const ::SwitchCommandRequest&>(request);
    if (cmd.key() != key()) return;

    const bool new_state = cmd.state();
    LVA_LOGI(kTag, "command: muted -> %s", new_state ? "true" : "false");

    state_.PersistMuted(new_state);
    state_.PlayMuteToggleSound(new_state);

    ::SwitchStateResponse resp;
    resp.set_key(key());
    resp.set_state(new_state);
    sink(lva::proto::kIdSwitchStateResponse, resp);
}

void MuteSwitchEntity::BroadcastState() {
    if (!state_.broadcast) return;
    ::SwitchStateResponse resp;
    resp.set_key(key());
    resp.set_state(state_.muted.load(std::memory_order_relaxed));
    state_.broadcast(lva::proto::kIdSwitchStateResponse, resp);
}

}  // namespace lva::entities
