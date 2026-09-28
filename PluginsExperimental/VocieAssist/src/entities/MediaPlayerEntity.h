
#pragma once

#include <cstdint>

#include "audio/IAudioPlayer.h"
#include "entities/Entity.h"

namespace lva::state { class ServerState; }

namespace lva::entities {

class MediaPlayerEntity final : public Entity {
   public:
    MediaPlayerEntity(std::uint32_t key,
                      lva::state::ServerState& state,
                      lva::audio::IAudioPlayer* player);

    void OnListEntities(const ResponseSink& sink) override;
    void OnSubscribeStates(const ResponseSink& sink) override;
    void OnCommand(const ::google::protobuf::MessageLite& request,
                   std::uint32_t request_msg_type_id,
                   const ResponseSink& sink) override;

    void BroadcastState();

   private:
    enum class PlayState { kIdle, kPlaying, kPaused };
    void EmitState(const ResponseSink& sink) const;
    void OnPlayerStateChanged(lva::audio::PlayerState s);

    lva::state::ServerState&  state_;
    lva::audio::IAudioPlayer* player_;     // not owned
    PlayState  play_state_      = PlayState::kIdle;
    bool       muted_           = false;
    double     pre_mute_volume_ = 1.0;
};

}  // namespace lva::entities
