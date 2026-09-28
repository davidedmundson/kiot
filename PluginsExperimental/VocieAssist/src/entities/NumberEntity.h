
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "entities/Entity.h"

namespace lva::entities {

class NumberEntity final : public Entity {
   public:
    using Getter = std::function<double()>;
    using Setter = std::function<void(double)>;

    struct Config {
        std::string object_id;
        std::string display_name;
        std::string icon;
        double min_value = 0.0;
        double max_value = 1.0;
        double step      = 0.001;
        int  mode_enum   = 1;  // BOX
        Getter getter;
        Setter setter;
    };

    NumberEntity(std::uint32_t key, Config cfg);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;
    void OnCommand(const ::google::protobuf::MessageLite& request,
                   std::uint32_t request_msg_type_id,
                   const ResponseSink& sink) override;

   private:
    Config cfg_;
};

}  // namespace lva::entities
