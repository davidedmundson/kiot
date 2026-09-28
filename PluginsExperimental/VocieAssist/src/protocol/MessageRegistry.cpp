#include "protocol/MessageRegistry.h"

namespace lva::proto {

std::string_view MessageName(std::uint32_t id) noexcept {
    switch (id) {
        case kIdHelloRequest:                          return "HelloRequest";
        case kIdHelloResponse:                         return "HelloResponse";
        case kIdAuthenticationRequest:                 return "AuthenticationRequest";
        case kIdAuthenticationResponse:                return "AuthenticationResponse";
        case kIdDisconnectRequest:                     return "DisconnectRequest";
        case kIdDisconnectResponse:                    return "DisconnectResponse";
        case kIdPingRequest:                           return "PingRequest";
        case kIdPingResponse:                          return "PingResponse";
        case kIdDeviceInfoRequest:                     return "DeviceInfoRequest";
        case kIdDeviceInfoResponse:                    return "DeviceInfoResponse";

        case kIdListEntitiesRequest:                   return "ListEntitiesRequest";
        case kIdListEntitiesDoneResponse:              return "ListEntitiesDoneResponse";
        case kIdSubscribeStatesRequest:                return "SubscribeStatesRequest";
        case kIdSubscribeHomeAssistantStatesRequest:   return "SubscribeHomeAssistantStatesRequest";

        case kIdListEntitiesSwitchResponse:            return "ListEntitiesSwitchResponse";
        case kIdSwitchStateResponse:                   return "SwitchStateResponse";
        case kIdSwitchCommandRequest:                  return "SwitchCommandRequest";

        case kIdListEntitiesButtonResponse:            return "ListEntitiesButtonResponse";
        case kIdButtonCommandRequest:                  return "ButtonCommandRequest";

        case kIdGetTimeRequest:                        return "GetTimeRequest";
        case kIdGetTimeResponse:                       return "GetTimeResponse";

        case kIdListEntitiesNumberResponse:            return "ListEntitiesNumberResponse";
        case kIdNumberStateResponse:                   return "NumberStateResponse";
        case kIdNumberCommandRequest:                  return "NumberCommandRequest";

        case kIdListEntitiesBinarySensorResponse:      return "ListEntitiesBinarySensorResponse";
        case kIdBinarySensorStateResponse:             return "BinarySensorStateResponse";

        case kIdListEntitiesSensorResponse:            return "ListEntitiesSensorResponse";
        case kIdSensorStateResponse:                   return "SensorStateResponse";
       
        case kIdLockStateResponse:                     return "LockStateResponse";
        case kIdListEntitiesLockResponse:              return "ListEntitiesLockResponse";
        case kIdLockCommandRequest:                    return "LockCommandRequest";

        case kIdListEntitiesTextResponse:              return "ListEntitiesTextResponse";
        case kIdTextStateResponse:                     return "TextStateResponse";
        case kIdTextCommandRequest:                    return "TextCommandRequest";

        
        case kIdListEntitiesSelectResponse:            return "ListEntitiesSelectResponse";
        case kIdSelectStateResponse:                   return "SelectStateResponse";
        case kIdSelectCommandRequest:                  return "SelectCommandRequest";

        case kIdListEntitiesMediaPlayerResponse:       return "ListEntitiesMediaPlayerResponse";
        case kIdMediaPlayerStateResponse:              return "MediaPlayerStateResponse";
        case kIdMediaPlayerCommandRequest:             return "MediaPlayerCommandRequest";

        case kIdListEntitiesEventResponse:             return "ListEntitiesEventResponse";
        case kIdEventResponse:                         return "EventResponse";

        case kIdListEntitiesUpdateResponse:            return "ListEntitiesUpdateResponse";
        case kIdUpdateStateResponse:                   return "UpdateStateResponse";
        case kIdUpdateCommandRequest:                  return "UpdateCommandRequest";

        case kIdSubscribeVoiceAssistantRequest:        return "SubscribeVoiceAssistantRequest";
        case kIdVoiceAssistantRequest:                 return "VoiceAssistantRequest";
        case kIdVoiceAssistantEventResponse:           return "VoiceAssistantEventResponse";
        case kIdVoiceAssistantAudio:                   return "VoiceAssistantAudio";
        case kIdVoiceAssistantTimerEventResponse:      return "VoiceAssistantTimerEventResponse";
        case kIdVoiceAssistantAnnounceRequest:         return "VoiceAssistantAnnounceRequest";
        case kIdVoiceAssistantAnnounceFinished:        return "VoiceAssistantAnnounceFinished";
        case kIdVoiceAssistantConfigurationRequest:    return "VoiceAssistantConfigurationRequest";
        case kIdVoiceAssistantConfigurationResponse:   return "VoiceAssistantConfigurationResponse";
        case kIdVoiceAssistantSetConfiguration:        return "VoiceAssistantSetConfiguration";


        default: return "?";
    }
}

}  // namespace lva::proto
