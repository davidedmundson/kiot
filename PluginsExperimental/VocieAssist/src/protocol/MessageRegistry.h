
#pragma once

#include <cstdint>
#include <string_view>

#include "protocol/proto_ids.h"

class HelloRequest;
class HelloResponse;
class AuthenticationRequest;
class AuthenticationResponse;
class DisconnectRequest;
class DisconnectResponse;
class PingRequest;
class PingResponse;
class DeviceInfoRequest;
class DeviceInfoResponse;
class ListEntitiesRequest;
class ListEntitiesDoneResponse;
class SubscribeStatesRequest;
class SubscribeHomeAssistantStatesRequest;
class ListEntitiesSwitchResponse;
class SwitchStateResponse;
class SwitchCommandRequest;
class ListEntitiesButtonResponse;
class ButtonCommandRequest;
class GetTimeRequest;
class GetTimeResponse;
class ListEntitiesNumberResponse;
class NumberStateResponse;
class NumberCommandRequest;
class ListEntitiesSelectResponse;
class SelectStateResponse;
class SelectCommandRequest;
class ListEntitiesMediaPlayerResponse;
class MediaPlayerStateResponse;
class MediaPlayerCommandRequest;
class ListEntitiesEventResponse;
class EventResponse;
class ListEntitiesUpdateResponse;
class UpdateStateResponse;
class UpdateCommandRequest;

class ListEntitiesBinarySensorResponse;
class BinarySensorStateResponse;

class ListEntitiesSensorResponse;
class SensorStateResponse;
//Lock entity
class LockStateResponse;
class ListEntitiesLockResponse;
class LockCommandRequest;
//text entity
class ListEntitiesTextResponse;
class TextStateResponse;
class TextCommandRequest;


// Voice assistant ().
class SubscribeVoiceAssistantRequest;
class VoiceAssistantRequest;
class VoiceAssistantEventResponse;
class VoiceAssistantAudio;
class VoiceAssistantTimerEventResponse;
class VoiceAssistantAnnounceRequest;
class VoiceAssistantAnnounceFinished;
class VoiceAssistantConfigurationRequest;
class VoiceAssistantConfigurationResponse;
class VoiceAssistantSetConfiguration;

namespace lva::proto {

template <typename T>
constexpr std::uint32_t MessageId();

// Macro to declare each (proto type, wire id) pair we use.
#define LVA_DECLARE_MESSAGE_ID(ProtoType, IdConst)               \
    template <>                                                  \
    inline constexpr std::uint32_t MessageId<ProtoType>() {      \
        return (IdConst);                                        \
    }

// Connection lifecycle ().
LVA_DECLARE_MESSAGE_ID(::HelloRequest,           kIdHelloRequest)
LVA_DECLARE_MESSAGE_ID(::HelloResponse,          kIdHelloResponse)
LVA_DECLARE_MESSAGE_ID(::AuthenticationRequest,  kIdAuthenticationRequest)
LVA_DECLARE_MESSAGE_ID(::AuthenticationResponse, kIdAuthenticationResponse)
LVA_DECLARE_MESSAGE_ID(::DisconnectRequest,      kIdDisconnectRequest)
LVA_DECLARE_MESSAGE_ID(::DisconnectResponse,     kIdDisconnectResponse)
LVA_DECLARE_MESSAGE_ID(::PingRequest,            kIdPingRequest)
LVA_DECLARE_MESSAGE_ID(::PingResponse,           kIdPingResponse)
LVA_DECLARE_MESSAGE_ID(::DeviceInfoRequest,      kIdDeviceInfoRequest)
LVA_DECLARE_MESSAGE_ID(::DeviceInfoResponse,     kIdDeviceInfoResponse)

// Entity discovery / state subscription ().
LVA_DECLARE_MESSAGE_ID(::ListEntitiesRequest,                   kIdListEntitiesRequest)
LVA_DECLARE_MESSAGE_ID(::ListEntitiesDoneResponse,              kIdListEntitiesDoneResponse)
LVA_DECLARE_MESSAGE_ID(::SubscribeStatesRequest,                kIdSubscribeStatesRequest)
LVA_DECLARE_MESSAGE_ID(::SubscribeHomeAssistantStatesRequest,
                       kIdSubscribeHomeAssistantStatesRequest)

// Switch entities ( Mute, ThinkingSound).
LVA_DECLARE_MESSAGE_ID(::ListEntitiesSwitchResponse, kIdListEntitiesSwitchResponse)
LVA_DECLARE_MESSAGE_ID(::SwitchStateResponse,        kIdSwitchStateResponse)
LVA_DECLARE_MESSAGE_ID(::SwitchCommandRequest,       kIdSwitchCommandRequest)

// Button entities ( "Check for updates").
LVA_DECLARE_MESSAGE_ID(::ListEntitiesButtonResponse, kIdListEntitiesButtonResponse)
LVA_DECLARE_MESSAGE_ID(::ButtonCommandRequest,       kIdButtonCommandRequest)

// HA time + timezone ().
LVA_DECLARE_MESSAGE_ID(::GetTimeRequest,             kIdGetTimeRequest)
LVA_DECLARE_MESSAGE_ID(::GetTimeResponse,            kIdGetTimeResponse)

// Number entities ( 3 sensitivities + mic_volume + mic_gain).
LVA_DECLARE_MESSAGE_ID(::ListEntitiesNumberResponse, kIdListEntitiesNumberResponse)
LVA_DECLARE_MESSAGE_ID(::NumberStateResponse,        kIdNumberStateResponse)
LVA_DECLARE_MESSAGE_ID(::NumberCommandRequest,       kIdNumberCommandRequest)

// Select entities ( mic_noise_suppression).
LVA_DECLARE_MESSAGE_ID(::ListEntitiesSelectResponse, kIdListEntitiesSelectResponse)
LVA_DECLARE_MESSAGE_ID(::SelectStateResponse,        kIdSelectStateResponse)
LVA_DECLARE_MESSAGE_ID(::SelectCommandRequest,       kIdSelectCommandRequest)

// Media player entity ().
LVA_DECLARE_MESSAGE_ID(::ListEntitiesMediaPlayerResponse, kIdListEntitiesMediaPlayerResponse)
LVA_DECLARE_MESSAGE_ID(::MediaPlayerStateResponse,        kIdMediaPlayerStateResponse)
LVA_DECLARE_MESSAGE_ID(::MediaPlayerCommandRequest,       kIdMediaPlayerCommandRequest)

// Event entity ( home button).
LVA_DECLARE_MESSAGE_ID(::ListEntitiesEventResponse, kIdListEntitiesEventResponse)
LVA_DECLARE_MESSAGE_ID(::EventResponse,             kIdEventResponse)

// Update entity ( firmware update placeholder).
LVA_DECLARE_MESSAGE_ID(::ListEntitiesUpdateResponse, kIdListEntitiesUpdateResponse)
LVA_DECLARE_MESSAGE_ID(::UpdateStateResponse,        kIdUpdateStateResponse)
LVA_DECLARE_MESSAGE_ID(::UpdateCommandRequest,       kIdUpdateCommandRequest)


// Binary sensor entities.
LVA_DECLARE_MESSAGE_ID(::ListEntitiesBinarySensorResponse, kIdListEntitiesBinarySensorResponse)
LVA_DECLARE_MESSAGE_ID(::BinarySensorStateResponse,        kIdBinarySensorStateResponse)


// Voice assistant pipeline ().
LVA_DECLARE_MESSAGE_ID(::VoiceAssistantRequest,                kIdVoiceAssistantRequest)
LVA_DECLARE_MESSAGE_ID(::VoiceAssistantEventResponse,          kIdVoiceAssistantEventResponse)
LVA_DECLARE_MESSAGE_ID(::VoiceAssistantAudio,                  kIdVoiceAssistantAudio)
LVA_DECLARE_MESSAGE_ID(::VoiceAssistantTimerEventResponse,     kIdVoiceAssistantTimerEventResponse)
LVA_DECLARE_MESSAGE_ID(::VoiceAssistantAnnounceRequest,        kIdVoiceAssistantAnnounceRequest)
LVA_DECLARE_MESSAGE_ID(::VoiceAssistantAnnounceFinished,       kIdVoiceAssistantAnnounceFinished)
LVA_DECLARE_MESSAGE_ID(::VoiceAssistantConfigurationRequest,   kIdVoiceAssistantConfigurationRequest)
LVA_DECLARE_MESSAGE_ID(::VoiceAssistantConfigurationResponse,  kIdVoiceAssistantConfigurationResponse)
LVA_DECLARE_MESSAGE_ID(::VoiceAssistantSetConfiguration,       kIdVoiceAssistantSetConfiguration)

#undef LVA_DECLARE_MESSAGE_ID

std::string_view MessageName(std::uint32_t msg_type_id) noexcept;

}  // namespace lva::proto
