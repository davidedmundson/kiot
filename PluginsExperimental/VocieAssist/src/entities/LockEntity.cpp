#include "entities/LockEntity.h"
#include "protocol/MessageRegistry.h"
#include "util/Log.h"
#include "api.pb.h"

namespace lva::entities {

namespace {
constexpr const char* kTag = "lock";
}

LockEntity::LockEntity(std::uint32_t key, Config cfg)
    : Entity(key, cfg.object_id, cfg.display_name), cfg_(std::move(cfg)) {}

void LockEntity::OnListEntities(const ResponseSink& sink) {
    ::ListEntitiesLockResponse resp;
    resp.set_object_id(object_id());
    resp.set_key(key());
    resp.set_name(name());
    if (!cfg_.icon.empty()) resp.set_icon(cfg_.icon);

    sink(lva::proto::kIdListEntitiesLockResponse, resp);
}

void LockEntity::OnSubscribeStates(const ResponseSink& sink) {
    bool locked = false;
    if (cfg_.get_state) {
        try { locked = cfg_.get_state(); } catch (...) {}
    }
    last_sent_state_ = locked;

    ::LockStateResponse resp;
    resp.set_key(key());
    resp.set_state(locked ? ::LOCK_STATE_LOCKED : ::LOCK_STATE_UNLOCKED);
    // Fjernet set_missing_state siden den ikke finnes her

    sink(lva::proto::kIdLockStateResponse, resp);
}

void LockEntity::OnCommand(const ::google::protobuf::MessageLite& request,
                           std::uint32_t request_msg_type_id,
                           const ResponseSink& /*sink*/) {
    if (request_msg_type_id != lva::proto::kIdLockCommandRequest) return;
    const auto& cmd = static_cast<const ::LockCommandRequest&>(request);
    if (cmd.key() != key()) return;

    // Sjekk kommandoen generisk eller bruk kommando-enum hvis den finnes. 
    // Ofte har cmd et felt som heter .command() eller .state(). 
    // Vi setter en trygg sjekk basert på hva kommando-enumene i ESPHome usually heter:
    bool should_lock = true; 
    
    // Hvis kommandofeltet er tilgjengelig, sjekk det (hvis ikke, sjekk hva cmd har):
    // (Hvis luer på hva cmd har av metoder, kan du sjekke api.pb.h etter LockCommandRequest)
    if (cmd.command() == ::LOCK_LOCK) {
        should_lock = true;
    } else {
        should_lock = false;
    }

    if (cfg_.on_command) {
        try {
            cfg_.on_command(should_lock);
        } catch (...) {}
    }
}

void LockEntity::UpdateState(bool locked, const ResponseSink& sink) {
    if (locked == last_sent_state_) return;
    last_sent_state_ = locked;

    ::LockStateResponse resp;
    resp.set_key(key());
    resp.set_state(locked ? ::LOCK_STATE_LOCKED : ::LOCK_STATE_UNLOCKED);
    // Fjernet set_missing_state

    sink(lva::proto::kIdLockStateResponse, resp);
}

} // namespace lva::entities