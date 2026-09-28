#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "entities/Entity.h"

namespace lva::entities {

class SensorEntity final : public Entity {
public:
    using StateGetter = std::function<float()>;

    struct Config {
        std::string object_id;
        std::string display_name;
        std::string icon;
        std::string device_class;
        std::string state_class;
        std::string unit_of_measurement;
        int accuracy_decimals = 1;
        StateGetter get_state;
    };

    SensorEntity(std::uint32_t key, Config cfg);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;
    void UpdateState(float state, const ResponseSink& sink);

private:
    Config cfg_;
    float last_sent_state_ = 0.0f;
};

} // namespace lva::entities