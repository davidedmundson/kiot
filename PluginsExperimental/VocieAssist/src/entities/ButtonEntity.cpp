#include "entities/ButtonEntity.h"

#include "protocol/MessageRegistry.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::entities {

namespace {
constexpr const char* kTag = "button";
}

ButtonEntity::ButtonEntity(std::uint32_t key, Config cfg)
    : Entity(key, cfg.object_id, cfg.display_name), cfg_(std::move(cfg)) {}

void ButtonEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesButtonResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    if (!cfg_.icon.empty()) resp.set_icon(cfg_.icon);
    if (!cfg_.device_class.empty()) {
        resp.set_device_class(cfg_.device_class);
    }
    if (cfg_.entity_category != 0) {
        resp.set_entity_category(
            static_cast<::EntityCategory>(cfg_.entity_category));
    }
    sink(lva::proto::kIdListEntitiesButtonResponse, resp);
}

void ButtonEntity::OnSubscribeStates(const ResponseSink& /*sink*/) {
    // Buttons are stateless — no state response.
}

void ButtonEntity::OnCommand(const ::google::protobuf::MessageLite& request,
                             std::uint32_t request_msg_type_id,
                             const ResponseSink& /*sink*/) {
    if (request_msg_type_id != lva::proto::kIdButtonCommandRequest) return;
    const auto& cmd = static_cast<const ::ButtonCommandRequest&>(request);
    if (cmd.key() != key()) return;
    LVA_LOGI(kTag, "[%s] pressed", object_id().c_str());
    if (cfg_.on_click) {
        try {
            cfg_.on_click();
        } catch (const std::exception& e) {
            LVA_LOGW(kTag, "[%s] on_click threw: %s",
                     object_id().c_str(), e.what());
        } catch (...) {
            LVA_LOGW(kTag, "[%s] on_click threw", object_id().c_str());
        }
    }
}

}  // namespace lva::entities
