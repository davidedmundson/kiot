#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "entities/Entity.h"

namespace lva::entities {

class TextEntity final : public Entity {
public:
    using StateGetter = std::function<std::string()>;
    using CommandHandler = std::function<void(const std::string&)>;

    struct Config {
        std::string object_id;
        std::string display_name;
        std::string icon;
        StateGetter get_state;
        CommandHandler on_command;
    };

    TextEntity(std::uint32_t key, Config cfg);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;
    void OnCommand(const ::google::protobuf::MessageLite& request,
                   std::uint32_t request_msg_type_id,
                   const ResponseSink& sink) override;
    void UpdateState(const std::string& state, const ResponseSink& sink);

private:
    Config cfg_;
    std::string last_sent_state_;
};

} // namespace lva::entities