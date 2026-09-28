#include "entities/NumberEntity.h"

#include "protocol/MessageRegistry.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::entities {

namespace {
constexpr const char* kTag = "number";
}

NumberEntity::NumberEntity(std::uint32_t key, Config cfg)
    : Entity(key, cfg.object_id, cfg.display_name), cfg_(std::move(cfg)) {}

void NumberEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesNumberResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    resp.set_entity_category(::ENTITY_CATEGORY_CONFIG);
    resp.set_min_value(static_cast<float>(cfg_.min_value));
    resp.set_max_value(static_cast<float>(cfg_.max_value));
    resp.set_step(static_cast<float>(cfg_.step));
    if (!cfg_.icon.empty()) {
        resp.set_icon(cfg_.icon);
    }
    resp.set_mode(static_cast<::NumberMode>(cfg_.mode_enum));
    sink(lva::proto::kIdListEntitiesNumberResponse, resp);
}

void NumberEntity::OnSubscribeStates(const ResponseSink& sink) {
    ::NumberStateResponse resp;
    resp.set_key(key());
    resp.set_state(static_cast<float>(cfg_.getter ? cfg_.getter()
                                                  : cfg_.min_value));
    sink(lva::proto::kIdNumberStateResponse, resp);
}

void NumberEntity::OnCommand(const ::google::protobuf::MessageLite& request,
                             std::uint32_t request_msg_type_id,
                             const ResponseSink& sink) {
    if (request_msg_type_id != lva::proto::kIdNumberCommandRequest) return;
    const auto& cmd = static_cast<const ::NumberCommandRequest&>(request);
    if (cmd.key() != key()) return;

    const double new_value = cmd.state();
    LVA_LOGI(kTag, "command: %s -> %.4f", object_id().c_str(), new_value);

    if (cfg_.setter) {
        cfg_.setter(new_value);
    }

    // Echo back the current (possibly clamped) value.
    ::NumberStateResponse resp;
    resp.set_key(key());
    resp.set_state(static_cast<float>(cfg_.getter ? cfg_.getter()
                                                  : new_value));
    sink(lva::proto::kIdNumberStateResponse, resp);
}

}  // namespace lva::entities
