#include "entities/BinarySensorEntity.h"

#include "protocol/MessageRegistry.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::entities {

namespace {
constexpr const char* kTag = "binary_sensor";
}

BinarySensorEntity::BinarySensorEntity(std::uint32_t key, Config cfg)
    : Entity(key, cfg.object_id, cfg.display_name), cfg_(std::move(cfg)) {}

void BinarySensorEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesBinarySensorResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    
    if (!cfg_.icon.empty()) resp.set_icon(cfg_.icon);
    if (!cfg_.device_class.empty()) {
        resp.set_device_class(cfg_.device_class);
    }
    resp.set_is_status_binary_sensor(cfg_.is_status_binary_sensor);
    resp.set_disabled_by_default(cfg_.disabled_by_default);

    sink(lva::proto::kIdListEntitiesBinarySensorResponse, resp);
}

void BinarySensorEntity::OnSubscribeStates(const ResponseSink& sink) {
    // Når Home Assistant abonnerer på tilstander, sender vi gjeldende verdi med en gang
    bool current_state = false;
    if (cfg_.get_state) {
        try {
            current_state = cfg_.get_state();
        } catch (...) {
            LVA_LOGW(kTag, "[%s] get_state threw during subscribe", object_id().c_str());
        }
    }
    last_sent_state_ = current_state;

    ::BinarySensorStateResponse resp;
    resp.set_key(key());
    resp.set_state(current_state);
    resp.set_missing_state(false);

    sink(lva::proto::kIdBinarySensorStateResponse, resp);
}

void BinarySensorEntity::UpdateState(bool state, const ResponseSink& sink) {
    if (state == last_sent_state_) return; // Send kun ved endring
    last_sent_state_ = state;

    ::BinarySensorStateResponse resp;
    resp.set_key(key());
    resp.set_state(state);
    resp.set_missing_state(false);

    sink(lva::proto::kIdBinarySensorStateResponse, resp);
}

} // namespace lva::entities