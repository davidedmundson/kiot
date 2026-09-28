#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "entities/Entity.h"

namespace lva::entities {

class BinarySensorEntity final : public Entity {
public:
    using StateGetter = std::function<bool()>;

    struct Config {
        std::string object_id;
        std::string display_name;
        std::string icon;
        std::string device_class;        // f.eks. "connectivity", "motion", "" for none
        bool is_status_binary_sensor = false;
        bool disabled_by_default = false;
        StateGetter get_state;          // Funksjon som returnerer aktuell bool-tilstand
    };

    BinarySensorEntity(std::uint32_t key, Config cfg);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;
    // Binary sensors are read-only from HA side, so OnCommand is not needed.

    // Kall denne for å pushe ny tilstand ut til Home Assistant når den endrer seg!
    void UpdateState(bool state, const ResponseSink& sink);

private:
    Config cfg_;
    bool last_sent_state_ = false;
};

} // namespace lva::entities