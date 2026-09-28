#include "entities/TextEntity.h"
#include "protocol/MessageRegistry.h"
#include "util/Log.h"
#include "api.pb.h"

namespace lva::entities {

namespace {
constexpr const char* kTag = "text";
}

TextEntity::TextEntity(std::uint32_t key, Config cfg)
    : Entity(key, cfg.object_id, cfg.display_name), cfg_(std::move(cfg)) {}

void TextEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesTextResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    if (!cfg_.icon.empty()) resp.set_icon(cfg_.icon);

    sink(lva::proto::kIdListEntitiesTextResponse, resp);
}

void TextEntity::OnSubscribeStates(const ResponseSink& sink) {
    std::string current_state = "";
    if (cfg_.get_state) {
        try { current_state = cfg_.get_state(); } catch (...) {}
    }
    last_sent_state_ = current_state;

    ::TextStateResponse resp;
    resp.set_key(key());
    resp.set_state(current_state);
    resp.set_missing_state(false);

    sink(lva::proto::kIdTextStateResponse, resp);
}

void TextEntity::OnCommand(const ::google::protobuf::MessageLite& request,
                           std::uint32_t request_msg_type_id,
                           const ResponseSink& /*sink*/) {
    if (request_msg_type_id != lva::proto::kIdTextCommandRequest) return;
    const auto& cmd = static_cast<const ::TextCommandRequest&>(request);
    if (cmd.key() != key()) return;

    if (cfg_.on_command) {
        try {
            cfg_.on_command(cmd.state());
        } catch (const std::exception& e) {
            LVA_LOGW(kTag, "[%s] text command threw: %s", object_id().c_str(), e.what());
        }
    }
}

void TextEntity::UpdateState(const std::string& state, const ResponseSink& sink) {
    if (state == last_sent_state_) return;
    last_sent_state_ = state;

    ::TextStateResponse resp;
    resp.set_key(key());
    resp.set_state(state);
    resp.set_missing_state(false);

    sink(lva::proto::kIdTextStateResponse, resp);
}

} // namespace lva::entities