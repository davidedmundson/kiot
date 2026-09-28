
#pragma once

#include <cstdint>

#include "entities/Entity.h"

namespace lva::state { class ServerState; }

namespace lva::entities {

class ThinkingSoundEntity final : public Entity {
   public:
    ThinkingSoundEntity(std::uint32_t key, lva::state::ServerState& state);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;
    void OnCommand(const ::google::protobuf::MessageLite& request,
                   std::uint32_t request_msg_type_id,
                   const ResponseSink& sink) override;

   private:
    lva::state::ServerState& state_;
};

}  // namespace lva::entities
