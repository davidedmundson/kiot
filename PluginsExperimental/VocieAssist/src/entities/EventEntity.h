
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "entities/Entity.h"

namespace lva::entities {

class EventEntity final : public Entity {
   public:
    struct Config {
        std::string object_id;
        std::string display_name;
        std::string icon;
        std::vector<std::string> event_types;
    };

    EventEntity(std::uint32_t key, Config cfg);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;

    void Trigger(const std::string& event_type,
                 const ResponseSink& sink);

    const std::vector<std::string>& event_types() const noexcept {
        return cfg_.event_types;
    }

   private:
    Config cfg_;
};

}  // namespace lva::entities
