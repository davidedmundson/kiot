
#pragma once

#include <cstdint>
#include <functional>
#include <google/protobuf/message_lite.h>
#include <string>

namespace lva::entities {

using ResponseSink = std::function<void(std::uint32_t msg_type_id,
                                        const ::google::protobuf::MessageLite& msg)>;

class Entity {
   public:
    Entity(std::uint32_t key, std::string object_id, std::string name)
        : key_(key),
          object_id_(std::move(object_id)),
          name_(std::move(name)) {}

    virtual ~Entity() = default;

    Entity(const Entity&)            = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&)                 = delete;
    Entity& operator=(Entity&&)      = delete;

    std::uint32_t key()             const noexcept { return key_; }
    const std::string& object_id()  const noexcept { return object_id_; }
    const std::string& name()       const noexcept { return name_; }

    virtual void OnListEntities(const ResponseSink& sink) = 0;

    // Emit the current *StateResponse. Always exactly one message.
    virtual void OnSubscribeStates(const ResponseSink& sink) = 0;

    virtual void OnCommand(const ::google::protobuf::MessageLite& /*request*/,
                           std::uint32_t /*request_msg_type_id*/,
                           const ResponseSink& /*sink*/) {}

   private:
    std::uint32_t key_;
    std::string object_id_;
    std::string name_;
};

}  // namespace lva::entities
