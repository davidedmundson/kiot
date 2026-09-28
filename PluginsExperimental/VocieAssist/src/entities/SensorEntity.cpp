#include "entities/SensorEntity.h"
#include "protocol/MessageRegistry.h"
#include "util/Log.h"
#include "api.pb.h"

namespace lva::entities {

namespace {
constexpr const char* kTag = "sensor";
}

SensorEntity::SensorEntity(std::uint32_t key, Config cfg)
    : Entity(key, cfg.object_id, cfg.display_name), cfg_(std::move(cfg)) {}

void SensorEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesSensorResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    
    if (!cfg_.icon.empty()) resp.set_icon(cfg_.icon);
    if (!cfg_.device_class.empty()) resp.set_device_class(cfg_.device_class);
    
    // if (!cfg_.state_class.empty()) resp.set_state_class(cfg_.state_class);
    
    resp.set_state_class(::SensorStateClass::STATE_CLASS_MEASUREMENT);

    if (!cfg_.unit_of_measurement.empty()) resp.set_unit_of_measurement(cfg_.unit_of_measurement);
    resp.set_accuracy_decimals(cfg_.accuracy_decimals);

    sink(lva::proto::kIdListEntitiesSensorResponse, resp);
}

void SensorEntity::OnSubscribeStates(const ResponseSink& sink) {
    float current_state = 0.0f;
    if (cfg_.get_state) {
        try { current_state = cfg_.get_state(); } catch (...) {}
    }
    last_sent_state_ = current_state;

    ::SensorStateResponse resp;
    resp.set_key(key());
    resp.set_state(current_state);
    resp.set_missing_state(false);

    sink(lva::proto::kIdSensorStateResponse, resp);
}

void SensorEntity::UpdateState(float state, const ResponseSink& sink) {
    if (state == last_sent_state_) return;
    last_sent_state_ = state;

    ::SensorStateResponse resp;
    resp.set_key(key());
    resp.set_state(state);
    resp.set_missing_state(false);

    sink(lva::proto::kIdSensorStateResponse, resp);
}

} // namespace lva::entities