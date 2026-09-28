#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "entities/Entity.h"

namespace lva::entities {

class LockEntity final : public Entity {
public:
    using StateGetter = std::function<bool()>; // true = locked, false = unlocked
    using CommandHandler = std::function<void(bool lock)>;

    struct Config {
        std::string object_id;
        std::string display_name;
        std::string icon;
        StateGetter get_state;
        CommandHandler on_command;
    };

    LockEntity(std::uint32_t key, Config cfg);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;
    void OnCommand(const ::google::protobuf::MessageLite& request,
                   std::uint32_t request_msg_type_id,
                   const ResponseSink& sink) override;
    void UpdateState(bool locked, const ResponseSink& sink);

private:
    Config cfg_;
    bool last_sent_state_ = false;
};

} // namespace lva::entities