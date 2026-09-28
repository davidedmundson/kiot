#include "entities/SelectEntity.h"

#include "protocol/MessageRegistry.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::entities {

namespace {
constexpr const char* kTag = "select";
}

SelectEntity::SelectEntity(std::uint32_t key, Config cfg)
    : Entity(key, cfg.object_id, cfg.display_name), cfg_(std::move(cfg)) {}

void SelectEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesSelectResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    resp.set_entity_category(::ENTITY_CATEGORY_CONFIG);
    if (!cfg_.icon.empty()) {
        resp.set_icon(cfg_.icon);
    }
    for (const auto& opt : cfg_.options) {
        resp.add_options(opt);
    }
    sink(lva::proto::kIdListEntitiesSelectResponse, resp);
}

void SelectEntity::OnSubscribeStates(const ResponseSink& sink) {
    ::SelectStateResponse resp;
    resp.set_key(key());
    resp.set_state(cfg_.getter ? cfg_.getter() : "");
    sink(lva::proto::kIdSelectStateResponse, resp);
}

void SelectEntity::OnCommand(const ::google::protobuf::MessageLite& request,
                             std::uint32_t request_msg_type_id,
                             const ResponseSink& sink) {
    if (request_msg_type_id != lva::proto::kIdSelectCommandRequest) return;
    const auto& cmd = static_cast<const ::SelectCommandRequest&>(request);
    if (cmd.key() != key()) return;

    const std::string& new_value = cmd.state();
    LVA_LOGI(kTag, "command: %s -> '%s'", object_id().c_str(),
             new_value.c_str());

    if (cfg_.setter) {
        cfg_.setter(new_value);
    }

    ::SelectStateResponse resp;
    resp.set_key(key());
    resp.set_state(cfg_.getter ? cfg_.getter() : new_value);
    sink(lva::proto::kIdSelectStateResponse, resp);
}

}  // namespace lva::entities
