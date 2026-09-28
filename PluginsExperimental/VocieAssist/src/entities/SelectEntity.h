
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "entities/Entity.h"

namespace lva::entities {

class SelectEntity final : public Entity {
   public:
    using Getter = std::function<std::string()>;
    using Setter = std::function<void(const std::string&)>;

    struct Config {
        std::string object_id;
        std::string display_name;
        std::string icon;
        std::vector<std::string> options;
        Getter getter;
        Setter setter;
    };

    SelectEntity(std::uint32_t key, Config cfg);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;
    void OnCommand(const ::google::protobuf::MessageLite& request,
                   std::uint32_t request_msg_type_id,
                   const ResponseSink& sink) override;

   private:
    Config cfg_;
};

}  // namespace lva::entities
