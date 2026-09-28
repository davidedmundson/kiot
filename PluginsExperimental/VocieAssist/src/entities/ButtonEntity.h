
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "entities/Entity.h"

namespace lva::entities {

class ButtonEntity final : public Entity {
   public:
    using ClickHandler = std::function<void()>;

    struct Config {
        std::string  object_id;
        std::string  display_name;
        std::string  icon;
        std::string  device_class;       // e.g. "restart", "" for none
        int          entity_category = 0; // 0=NONE, 1=CONFIG, 2=DIAGNOSTIC
        ClickHandler on_click;
    };

    ButtonEntity(std::uint32_t key, Config cfg);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;
    void OnCommand(const ::google::protobuf::MessageLite& request,
                   std::uint32_t request_msg_type_id,
                   const ResponseSink& sink) override;

   private:
    Config cfg_;
};

}  // namespace lva::entities
