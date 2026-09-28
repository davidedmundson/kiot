#include "entities/ThinkingSoundEntity.h"

#include "protocol/MessageRegistry.h"
#include "state/ServerState.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::entities {

namespace {
constexpr const char* kTag = "think_sw";
constexpr const char* kObjectId = "thinking_sound";
constexpr const char* kDisplayName = "Thinking Sound";
}  // namespace

ThinkingSoundEntity::ThinkingSoundEntity(std::uint32_t key,
                                         lva::state::ServerState& state)
    : Entity(key, kObjectId, kDisplayName), state_(state) {}

void ThinkingSoundEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesSwitchResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    resp.set_entity_category(::ENTITY_CATEGORY_CONFIG);
    resp.set_icon("mdi:music-note");
    sink(lva::proto::kIdListEntitiesSwitchResponse, resp);
}

void ThinkingSoundEntity::OnSubscribeStates(const ResponseSink& sink) {
    ::SwitchStateResponse resp;
    resp.set_key(key());
    resp.set_state(state_.preferences.thinking_sound != 0);
    sink(lva::proto::kIdSwitchStateResponse, resp);
}

void ThinkingSoundEntity::OnCommand(
    const ::google::protobuf::MessageLite& request,
    std::uint32_t request_msg_type_id,
    const ResponseSink& sink) {
    if (request_msg_type_id != lva::proto::kIdSwitchCommandRequest) return;
    const auto& cmd = static_cast<const ::SwitchCommandRequest&>(request);
    if (cmd.key() != key()) return;

    const bool new_state = cmd.state();
    LVA_LOGI(kTag, "command: thinking_sound -> %s",
             new_state ? "true" : "false");

    state_.PersistThinkingSound(new_state);

    ::SwitchStateResponse resp;
    resp.set_key(key());
    resp.set_state(new_state);
    sink(lva::proto::kIdSwitchStateResponse, resp);
}

}  // namespace lva::entities
