#include "entities/EventEntity.h"

#include <algorithm>

#include "protocol/MessageRegistry.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::entities {

namespace {
constexpr const char* kTag = "event";
}

EventEntity::EventEntity(std::uint32_t key, Config cfg)
    : Entity(key, cfg.object_id, cfg.display_name), cfg_(std::move(cfg)) {}

void EventEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesEventResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    if (!cfg_.icon.empty()) {
        resp.set_icon(cfg_.icon);
    }
    for (const auto& evt : cfg_.event_types) {
        resp.add_event_types(evt);
    }
    sink(lva::proto::kIdListEntitiesEventResponse, resp);
}

void EventEntity::OnSubscribeStates(const ResponseSink& /*sink*/) {
}

void EventEntity::Trigger(const std::string& event_type,
                          const ResponseSink& sink) {
    const bool known = std::any_of(
        cfg_.event_types.begin(), cfg_.event_types.end(),
        [&](const std::string& s) { return s == event_type; });
    if (!known) {
        LVA_LOGW(kTag, "[%s] unknown event_type '%s' — dropping",
                 object_id().c_str(), event_type.c_str());
        return;
    }

    ::EventResponse resp;
    resp.set_key(key());
    resp.set_event_type(event_type);
    sink(lva::proto::kIdEventResponse, resp);
    LVA_LOGI(kTag, "[%s] fired event: %s", object_id().c_str(),
             event_type.c_str());
}

}  // namespace lva::entities
